// main.cpp
#include "Fuzzer.hpp"
#include <cstdio>
#include <cstring>
void printHex(const char* label, const uint8_t* data, size_t size) {
    printf("[%s] ", label);
    for (size_t i = 0; i < size; ++i) {
        printf("%02X ", data[i]);
    }

    printf(" | ");

    for (size_t i = 0; i < size; ++i) {
        if (data[i] >= 32 && data[i] <= 126)
            printf("%c", data[i]);  // ASCII vizibil
        else
            printf(".");
    }

    printf("\n");
}

int main() {
    const char* inputStr = "Hellor";
    size_t inputLen = std::strlen(inputStr);
    size_t outLen = 0;

    uint8_t* input = new uint8_t[inputLen];
    std::memcpy(input, inputStr, inputLen);

    FuzzerCore random(FUZZSTYLE_RANDOMIZATION);
    uint8_t* randBuf = random.pass(input, inputLen, outLen);
    printHex("PASS", randBuf, outLen);

    randBuf = random.preFuzzing(input, inputLen, outLen);
    printHex("PRE", randBuf, outLen);

    randBuf = random.postFuzzing(input, inputLen, outLen);
    printHex("POST", randBuf, outLen);

    randBuf = random.fullFuzzing(input, inputLen, outLen);
    printHex("FULL", randBuf, outLen);

    randBuf = random.guidedFuzzing(input, inputLen, outLen);
    printHex("GUIDED", randBuf, outLen);

    delete[] input;
    return 0;
}
