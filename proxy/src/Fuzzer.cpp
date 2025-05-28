// FuzzerCore.cpp
#include "Fuzzer.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>

FuzzerCore::FuzzerCore(FuzzStyle style) : style(style) {
    for (int i = 0; i < MAX_RADAMSA_ARGS; ++i) radamsaArgs[i] = nullptr;
    configureStyleArgs();
}

void FuzzerCore::addArg(const char* arg) {
    if (argCount < MAX_RADAMSA_ARGS - 1) {
        radamsaArgs[argCount++] = arg;
        radamsaArgs[argCount] = nullptr;  // null terminate
    }
}

void FuzzerCore::configureStyleArgs() {
    addArg("radamsa");

    // Argumente comune
    addArg("-n"); addArg("1");  // Un singur output la fiecare execuție
    addArg("-g"); addArg("stdin=1048576");  // max 1MB din stdin
    addArg("-p"); addArg("od,nd=2,bu");

    switch (style) {
        case FUZZSTYLE_RANDOMIZATION:
            break;
        case FUZZSTYLE_TRUNCATE:
            addArg("-m"); addArg("td,tr2,ts1");
            break;
        case FUZZSTYLE_INSERT:
            addArg("-m"); addArg("li,lp,ls,lis");
            break;
        case FUZZSTYLE_OVERFLOW:
            addArg("-m"); addArg("bd,bf,br,bp");
            break;
        case FUZZSTYLE_CUSTOM:
            addArg("-m"); addArg("ab,xp=9,bei,ber,uw");
            break;
    }
}

uint8_t* FuzzerCore::runRadamsa(const uint8_t* data, size_t size, size_t& outSize) {
    int in_pipe[2], out_pipe[2];
    pipe(in_pipe);
    pipe(out_pipe);

    pid_t pid = fork();
    if (pid == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[1]);
        close(out_pipe[0]);
        execvp("./radamsa", (char* const*)radamsaArgs);
        perror("execvp failed");
        _exit(1);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);
    write(in_pipe[1], data, size);
    close(in_pipe[1]);

    uint8_t* tempBuf = (uint8_t*)malloc(MAX_BUFFER_SIZE);
    ssize_t readBytes = read(out_pipe[0], tempBuf, MAX_BUFFER_SIZE);
    close(out_pipe[0]);
    waitpid(pid, nullptr, 0);

    if (readBytes <= 0) {
        free(tempBuf);
        outSize = 0;
        return nullptr;
    }

    // Normalize size between 4KB and 1MB
    size_t normalizedLen = 0;
    uint8_t* finalBuf = normalizeOutputSize(tempBuf, readBytes, normalizedLen);
    free(tempBuf);
    outSize = normalizedLen;
    return finalBuf;
}

uint8_t* FuzzerCore::preFuzzing(const uint8_t* input, size_t size, size_t& newSize) {
    size_t fuzzSize;
    uint8_t* fuzz = runRadamsa(input, size, fuzzSize);
    uint8_t* output = (uint8_t*)malloc(fuzzSize + size);
    memcpy(output, fuzz, fuzzSize);
    memcpy(output + fuzzSize, input, size);
    newSize = fuzzSize + size;
    free(fuzz);
    return output;
}

uint8_t* FuzzerCore::postFuzzing(const uint8_t* input, size_t size, size_t& newSize) {
    size_t fuzzSize;
    uint8_t* fuzz = runRadamsa(input, size, fuzzSize);
    uint8_t* output = (uint8_t*)malloc(fuzzSize + size);
    memcpy(output, input, size);
    memcpy(output + size, fuzz, fuzzSize);
    newSize = fuzzSize + size;
    free(fuzz);
    return output;
}

uint8_t* FuzzerCore::fullFuzzing(const uint8_t* input, size_t size, size_t& newSize) {
    uint8_t* first = preFuzzing(input, size, newSize);
    uint8_t* second = postFuzzing(first, newSize, newSize);
    free(first);
    return second;
}

uint8_t* FuzzerCore::guidedFuzzing(const uint8_t* input, size_t size, size_t& newSize) {
    printf("[TODO] guidedFuzzing not implemented yet\n");
    newSize = size;
    uint8_t* copy = (uint8_t*)malloc(size);
    memcpy(copy, input, size);
    return copy;
}

uint8_t* FuzzerCore::pass(const uint8_t* input, size_t size, size_t& newSize) {
    newSize = size;
    uint8_t* copy = (uint8_t*)malloc(size);
    memcpy(copy, input, size);
    return copy;
}

uint8_t* FuzzerCore::normalizeOutputSize(uint8_t* input, size_t input_len, size_t& output_len) {
    if (input_len >= MIN_OUTPUT_SIZE && input_len <= MAX_BUFFER_SIZE) {
        output_len = input_len;
        uint8_t* copy = new uint8_t[output_len];
        std::memcpy(copy, input, output_len);
        return copy;
    }

    size_t new_len = input_len;
    while (new_len < MIN_OUTPUT_SIZE) {
        new_len += input_len;
    }
    if (new_len > MAX_BUFFER_SIZE) {
        new_len = MAX_BUFFER_SIZE;
    }

    uint8_t* result = new uint8_t[new_len];
    size_t copied = 0;
    while (copied + input_len <= new_len) {
        std::memcpy(result + copied, input, input_len);
        copied += input_len;
    }

    if (copied < new_len) {
        std::memcpy(result + copied, input, new_len - copied);
    }

    output_len = new_len;
    return result;
}