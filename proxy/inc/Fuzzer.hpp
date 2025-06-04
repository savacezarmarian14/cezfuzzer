// FuzzerCore.hpp
#ifndef FUZZER_CORE_HPP
#define FUZZER_CORE_HPP

#include <cstddef>
#include <cstdint>

#define FUZZ_LENGTH_MULTIPLIER 15
#define MAX_RADAMSA_ARGS       20
#define MIN_OUTPUT_SIZE        (100 * BATCH)
#define BATCH                  4096
#define MAX_BUFFER_SIZE        (10000 * BATCH)

enum FuzzStyle { FUZZSTYLE_RANDOMIZATION, FUZZSTYLE_TRUNCATE, FUZZSTYLE_INSERT, FUZZSTYLE_OVERFLOW, FUZZSTYLE_CUSTOM };

class FuzzerCore {
  public:
    FuzzerCore(FuzzStyle style = FUZZSTYLE_RANDOMIZATION);

    uint8_t* preFuzzing(const uint8_t* input, size_t size, size_t& newSize);
    uint8_t* postFuzzing(const uint8_t* input, size_t size, size_t& newSize);
    uint8_t* fullFuzzing(const uint8_t* input, size_t size, size_t& newSize);
    uint8_t* guidedFuzzing(const uint8_t* input, size_t size, size_t& newSize);
    uint8_t* pass(const uint8_t* input, size_t size, size_t& newSize);
    uint8_t* normalizeOutputSize(uint8_t* input, size_t input_len, size_t& output_len);
    uint8_t* runRadamsaExpanded(const uint8_t* data, size_t size, size_t& outSize);

  private:
    FuzzStyle   style;
    const char* radamsaArgs[MAX_RADAMSA_ARGS];
    int         argCount = 0;

    void     configureStyleArgs();
    void     addArg(const char* arg);
    uint8_t* runRadamsa(const uint8_t* data, size_t size, size_t& outSize);
};

#endif // FUZZER_CORE_HPP
