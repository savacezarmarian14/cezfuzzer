// FuzzerCore.cpp
#include "Fuzzer.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <algorithm>

/**
 * FuzzerCore implementation ensures that every fuzzed output buffer
 * has a final length between MIN_OUTPUT_SIZE and MAX_BUFFER_SIZE
 * (as defined in FuzzerCore.hpp).
 *
 * Modified behaviors:
 *  - postFuzzing: apply fuzz, then return [original message][fuzz]
 *  - preFuzzing: apply fuzz, then return [fuzz][original message]
 *  - fullFuzzing: apply fuzz twice, then return [fuzz1][original message][fuzz2]
 *  - pass: return the original message exactly (no padding/truncation)
 */

FuzzerCore::FuzzerCore(FuzzStyle style) : style(style)
{
    for (int i = 0; i < MAX_RADAMSA_ARGS; ++i) {
        radamsaArgs[i] = nullptr;
    }
    configureStyleArgs();
}

void FuzzerCore::addArg(const char* arg)
{
    if (argCount < MAX_RADAMSA_ARGS - 1) {
        radamsaArgs[argCount++] = arg;
        radamsaArgs[argCount]   = nullptr; // ensure null termination
    }
}

void FuzzerCore::configureStyleArgs()
{
    // Base executable
    addArg("radamsa");

    // Common radamsa arguments
    addArg("-n");
    addArg("1"); // produce exactly one output per invocation
    addArg("-g");
    addArg("stdin=1048576"); // allow up to 1MB from stdin
    addArg("-p");
    addArg("od,nd=2,bu"); // use octal dump & binary unit transport

    // Add mode-specific flags
    switch (style) {
        case FUZZSTYLE_RANDOMIZATION:
            // no extra flags
            break;
        case FUZZSTYLE_TRUNCATE:
            addArg("-m");
            addArg("td,tr2,ts1");
            break;
        case FUZZSTYLE_INSERT:
            addArg("-m");
            addArg("li,lp,ls,lis");
            break;
        case FUZZSTYLE_OVERFLOW:
            addArg("-m");
            addArg("bd,bf,br,bp");
            break;
        case FUZZSTYLE_CUSTOM:
            addArg("-m");
            addArg("ab,xp=9,bei,ber,uw");
            break;
    }
}

/**
 * runRadamsa:
 *   - Forks a child process to exec "./radamsa" with configured arguments.
 *   - Writes `data` to child's stdin.
 *   - Reads up to MAX_BUFFER_SIZE bytes from child's stdout into a temporary buffer.
 *   - Normalizes that buffer to lie between MIN_OUTPUT_SIZE and MAX_BUFFER_SIZE.
 *   - Returns a malloc'ed buffer of length outSize, or nullptr on failure.
 */
uint8_t* FuzzerCore::runRadamsa(const uint8_t* data, size_t size, size_t& outSize)
{
    int in_pipe[2], out_pipe[2];
    if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0) {
        perror("pipe failed");
        outSize = 0;
        return nullptr;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        outSize = 0;
        return nullptr;
    }

    if (pid == 0) {
        // Child: redirect stdin/stdout to pipes, then exec radamsa
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[1]);
        close(out_pipe[0]);
        execvp("./radamsa", (char* const*) radamsaArgs);
        perror("execvp failed");
        _exit(1);
    }

    // Parent: close unused ends, write input, read output
    close(in_pipe[0]);
    close(out_pipe[1]);
    write(in_pipe[1], data, size);
    close(in_pipe[1]);

    uint8_t* tempBuf = (uint8_t*) malloc(MAX_BUFFER_SIZE);
    if (!tempBuf) {
        perror("malloc failed");
        outSize = 0;
        close(out_pipe[0]);
        waitpid(pid, nullptr, 0);
        return nullptr;
    }

    ssize_t readBytes = read(out_pipe[0], tempBuf, MAX_BUFFER_SIZE);
    close(out_pipe[0]);
    waitpid(pid, nullptr, 0);

    if (readBytes <= 0) {
        // No data or error reading
        free(tempBuf);
        outSize = 0;
        return nullptr;
    }

    // Normalize length into [MIN_OUTPUT_SIZE, MAX_BUFFER_SIZE]
    size_t   normalizedLen = 0;
    uint8_t* finalBuf      = normalizeOutputSize(tempBuf, (size_t) readBytes, normalizedLen);
    free(tempBuf);

    outSize = normalizedLen;
    return finalBuf;
}

/**
 * runRadamsaExpanded:
 *   - Calls runRadamsa once to obtain an initial fuzz buffer.
 *   - If initial fuzz size >= MIN_OUTPUT_SIZE, return it directly (with normalization).
 *   - If initial fuzz size < MIN_OUTPUT_SIZE, repeatedly concatenate
 *     copies of the original input until reaching at least MIN_OUTPUT_SIZE.
 *   - Ensures that outSize is between MIN_OUTPUT_SIZE and MAX_BUFFER_SIZE.
 *   - Returns a malloc'ed buffer of length outSize, or nullptr on failure.
 */
uint8_t* FuzzerCore::runRadamsaExpanded(const uint8_t* data, size_t size, size_t& outSize)
{
    // First invocation of runRadamsa
    size_t   firstSize = 0;
    uint8_t* firstFuzz = runRadamsa(data, size, firstSize);
    if (!firstFuzz || firstSize == 0) {
        // If runRadamsa failed or returned zero, fallback to just repeating input
        if (firstFuzz)
            free(firstFuzz);

        // Build a buffer of MIN_OUTPUT_SIZE by repeating `data`
        size_t   target = MIN_OUTPUT_SIZE;
        uint8_t* result = (uint8_t*) malloc(target);
        if (!result) {
            perror("malloc failed");
            outSize = 0;
            return nullptr;
        }
        size_t copied = 0;
        while (copied < target) {
            size_t toCopy = std::min(size, target - copied);
            memcpy(result + copied, data, toCopy);
            copied += toCopy;
        }
        outSize = target;
        return result;
    }

    // If first run already meets minimum, but might exceed maximum, normalize
    if (firstSize >= MIN_OUTPUT_SIZE) {
        // If firstSize > MAX_BUFFER_SIZE, truncate via normalizeOutputSize
        size_t   normalizedLen = 0;
        uint8_t* normalized    = normalizeOutputSize(firstFuzz, firstSize, normalizedLen);
        free(firstFuzz);
        outSize = normalizedLen;
        return normalized;
    }

    // firstSize < MIN_OUTPUT_SIZE: build up to MIN_OUTPUT_SIZE by concatenating original input
    size_t target = MIN_OUTPUT_SIZE;
    if (target > MAX_BUFFER_SIZE) {
        target = MAX_BUFFER_SIZE;
    }
    uint8_t* expanded = (uint8_t*) malloc(target);
    if (!expanded) {
        perror("malloc failed");
        free(firstFuzz);
        outSize = 0;
        return nullptr;
    }

    // Copy the first fuzz chunk
    size_t copied = 0;
    memcpy(expanded, firstFuzz, firstSize);
    copied += firstSize;
    free(firstFuzz);

    // Fill the remainder with repeats of input
    while (copied < target) {
        size_t toCopy = std::min(size, target - copied);
        memcpy(expanded + copied, data, toCopy);
        copied += toCopy;
    }

    outSize = copied;
    return expanded;
}

/**
 * normalizeOutputSize:
 *   - Ensures that an input buffer of length input_len is transformed into
 *     a buffer whose length lies in [MIN_OUTPUT_SIZE, MAX_BUFFER_SIZE].
 *   - If input_len is already within that range, returns a copy of the input.
 *   - If input_len < MIN_OUTPUT_SIZE, repeats the input until reaching MIN_OUTPUT_SIZE.
 *   - If input_len > MAX_BUFFER_SIZE, truncates to MAX_BUFFER_SIZE.
 */
uint8_t* FuzzerCore::normalizeOutputSize(uint8_t* input, size_t input_len, size_t& output_len)
{
    if (input_len >= MIN_OUTPUT_SIZE && input_len <= MAX_BUFFER_SIZE) {
        // Already in range: return a copy
        output_len    = input_len;
        uint8_t* copy = (uint8_t*) malloc(output_len);
        if (copy) {
            memcpy(copy, input, output_len);
        } else {
            perror("malloc failed");
            output_len = 0;
        }
        return copy;
    }

    if (input_len < MIN_OUTPUT_SIZE) {
        // Expand by repeating until reaching at least MIN_OUTPUT_SIZE
        size_t target = MIN_OUTPUT_SIZE;
        if (target > MAX_BUFFER_SIZE) {
            target = MAX_BUFFER_SIZE;
        }
        uint8_t* expanded = (uint8_t*) malloc(target);
        if (!expanded) {
            perror("malloc failed");
            output_len = 0;
            return nullptr;
        }
        size_t copied = 0;
        while (copied < target) {
            size_t toCopy = std::min(input_len, target - copied);
            memcpy(expanded + copied, input, toCopy);
            copied += toCopy;
        }
        output_len = copied;
        return expanded;
    }

    // input_len > MAX_BUFFER_SIZE: truncate
    size_t   target    = MAX_BUFFER_SIZE;
    uint8_t* truncated = (uint8_t*) malloc(target);
    if (!truncated) {
        perror("malloc failed");
        output_len = 0;
        return nullptr;
    }
    memcpy(truncated, input, target);
    output_len = target;
    return truncated;
}

/**
 * postFuzzing:
 *   - Generate a fuzz buffer from the original input.
 *   - Concatenate [original input][fuzz].
 *   - Normalize the combined buffer size into [MIN_OUTPUT_SIZE, MAX_BUFFER_SIZE].
 *   - Return a malloc'ed buffer of length newSize, or nullptr on failure.
 */
uint8_t* FuzzerCore::postFuzzing(const uint8_t* input, size_t size, size_t& newSize)
{
    // Generate fuzz from the input
    size_t   fuzzSize = 0;
    uint8_t* fuzzed   = runRadamsaExpanded(input, size, fuzzSize);

    if (!fuzzed) {
        // If fuzz generation failed, just return a copy of the original input
        uint8_t* copy = (uint8_t*) malloc(size);
        if (!copy) {
            perror("malloc failed");
            newSize = 0;
            return nullptr;
        }
        memcpy(copy, input, size);
        newSize = size;
        return copy;
    }

    // Concatenate [original][fuzz]
    size_t   combinedLen = size + fuzzSize;
    uint8_t* combined    = (uint8_t*) malloc(combinedLen);
    if (!combined) {
        perror("malloc failed");
        free(fuzzed);
        newSize = 0;
        return nullptr;
    }
    memcpy(combined, input, size);
    memcpy(combined + size, fuzzed, fuzzSize);
    free(fuzzed);

    // Normalize the combined buffer into [MIN_OUTPUT_SIZE, MAX_BUFFER_SIZE]
    size_t   normalizedLen = 0;
    uint8_t* normalized    = normalizeOutputSize(combined, combinedLen, normalizedLen);
    free(combined);

    newSize = normalizedLen;
    return normalized;
}

/**
 * preFuzzing:
 *   - Generate a fuzz buffer from the original input.
 *   - Concatenate [fuzz][original input].
 *   - Normalize the combined buffer size into [MIN_OUTPUT_SIZE, MAX_BUFFER_SIZE].
 *   - Return a malloc'ed buffer of length newSize, or nullptr on failure.
 */
uint8_t* FuzzerCore::preFuzzing(const uint8_t* input, size_t size, size_t& newSize)
{
    // Generate fuzz from the input
    size_t   fuzzSize = 0;
    uint8_t* fuzzed   = runRadamsaExpanded(input, size, fuzzSize);

    if (!fuzzed) {
        // If fuzz generation failed, just return a copy of the original input
        uint8_t* copy = (uint8_t*) malloc(size);
        if (!copy) {
            perror("malloc failed");
            newSize = 0;
            return nullptr;
        }
        memcpy(copy, input, size);
        newSize = size;
        return copy;
    }

    // Concatenate [fuzz][original]
    size_t   combinedLen = fuzzSize + size;
    uint8_t* combined    = (uint8_t*) malloc(combinedLen);
    if (!combined) {
        perror("malloc failed");
        free(fuzzed);
        newSize = 0;
        return nullptr;
    }
    memcpy(combined, fuzzed, fuzzSize);
    memcpy(combined + fuzzSize, input, size);
    free(fuzzed);

    // Normalize the combined buffer into [MIN_OUTPUT_SIZE, MAX_BUFFER_SIZE]
    size_t   normalizedLen = 0;
    uint8_t* normalized    = normalizeOutputSize(combined, combinedLen, normalizedLen);
    free(combined);

    newSize = normalizedLen;
    return normalized;
}

/**
 * fullFuzzing:
 *   - Generate two fuzz buffers from the original input.
 *   - Concatenate [fuzz1][original input][fuzz2].
 *   - Normalize the combined buffer size into [MIN_OUTPUT_SIZE, MAX_BUFFER_SIZE].
 *   - Return a malloc'ed buffer of length newSize, or nullptr on failure.
 */
uint8_t* FuzzerCore::fullFuzzing(const uint8_t* input, size_t size, size_t& newSize)
{
    // Generate first fuzz
    size_t   fuzz1Size = 0;
    uint8_t* fuzz1     = runRadamsaExpanded(input, size, fuzz1Size);

    // Generate second fuzz
    size_t   fuzz2Size = 0;
    uint8_t* fuzz2     = runRadamsaExpanded(input, size, fuzz2Size);

    // If both fuzz attempts fail, return a copy of the original input
    if (!fuzz1 && !fuzz2) {
        uint8_t* copy = (uint8_t*) malloc(size);
        if (!copy) {
            perror("malloc failed");
            newSize = 0;
            return nullptr;
        }
        memcpy(copy, input, size);
        newSize = size;
        return copy;
    }

    // Treat individual failures as zero-length fuzz
    if (!fuzz1) {
        fuzz1Size = 0;
    }
    if (!fuzz2) {
        fuzz2Size = 0;
    }

    // Concatenate [fuzz1][original][fuzz2]
    size_t   combinedLen = fuzz1Size + size + fuzz2Size;
    uint8_t* combined    = (uint8_t*) malloc(combinedLen);
    if (!combined) {
        perror("malloc failed");
        if (fuzz1)
            free(fuzz1);
        if (fuzz2)
            free(fuzz2);
        newSize = 0;
        return nullptr;
    }
    size_t offset = 0;
    if (fuzz1Size > 0) {
        memcpy(combined + offset, fuzz1, fuzz1Size);
        offset += fuzz1Size;
    }
    memcpy(combined + offset, input, size);
    offset += size;
    if (fuzz2Size > 0) {
        memcpy(combined + offset, fuzz2, fuzz2Size);
    }
    if (fuzz1)
        free(fuzz1);
    if (fuzz2)
        free(fuzz2);

    // Normalize the combined buffer into [MIN_OUTPUT_SIZE, MAX_BUFFER_SIZE]
    size_t   normalizedLen = 0;
    uint8_t* normalized    = normalizeOutputSize(combined, combinedLen, normalizedLen);
    free(combined);

    newSize = normalizedLen;
    return normalized;
}

/**
 * guidedFuzzing:
 *   - Not modified: returns a copy of input, padded with zeros if size < MIN_OUTPUT_SIZE
 *     or truncated if size > MAX_BUFFER_SIZE.
 */
uint8_t* FuzzerCore::guidedFuzzing(const uint8_t* input, size_t size, size_t& newSize)
{
    size_t target = size;
    if (target < MIN_OUTPUT_SIZE) {
        target = MIN_OUTPUT_SIZE;
    } else if (target > MAX_BUFFER_SIZE) {
        target = MAX_BUFFER_SIZE;
    }
    uint8_t* buffer = (uint8_t*) malloc(target);
    if (!buffer) {
        perror("malloc failed");
        newSize = 0;
        return nullptr;
    }
    // Copy input
    size_t toCopy = std::min(size, target);
    memcpy(buffer, input, toCopy);
    // If padded, fill rest with zeros
    if (toCopy < target) {
        memset(buffer + toCopy, 0, target - toCopy);
    }
    newSize = target;
    return buffer;
}

/**
 * pass:
 *   - Returns an exact copy of the original input (no padding or truncation).
 */
uint8_t* FuzzerCore::pass(const uint8_t* input, size_t size, size_t& newSize)
{
    uint8_t* buffer = (uint8_t*) malloc(size);
    if (!buffer) {
        perror("malloc failed");
        newSize = 0;
        return nullptr;
    }
    memcpy(buffer, input, size);
    newSize = size;
    return buffer;
}
