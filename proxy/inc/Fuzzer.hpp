// FuzzerCore.hpp
#ifndef FUZZER_CORE_HPP
#define FUZZER_CORE_HPP

#include <cstddef>
#include <cstdint>

#define MAX_RADAMSA_ARGS 10
#define MAX_BUFFER_SIZE (4 * 1024)
#define MIN_OUTPUT_SIZE 4096

enum FuzzStyle {
    FUZZSTYLE_RANDOMIZATION,
    FUZZSTYLE_TRUNCATE,
    FUZZSTYLE_INSERT,
    FUZZSTYLE_OVERFLOW,
    FUZZSTYLE_CUSTOM
};

class FuzzerCore {
public:
    FuzzerCore(FuzzStyle style = FUZZSTYLE_RANDOMIZATION);

    uint8_t* preFuzzing(const uint8_t* input, size_t size, size_t& newSize);
    uint8_t* postFuzzing(const uint8_t* input, size_t size, size_t& newSize);
    uint8_t* fullFuzzing(const uint8_t* input, size_t size, size_t& newSize);
    uint8_t* guidedFuzzing(const uint8_t* input, size_t size, size_t& newSize);
    uint8_t* pass(const uint8_t* input, size_t size, size_t& newSize);
    uint8_t* normalizeOutputSize(uint8_t* input, size_t input_len, size_t& output_len);
private:
    FuzzStyle style;
    const char* radamsaArgs[MAX_RADAMSA_ARGS];
    int argCount = 0;

    void configureStyleArgs();
    void addArg(const char* arg);
    uint8_t* runRadamsa(const uint8_t* data, size_t size, size_t& outSize);
};

#endif // FUZZER_CORE_HPP
