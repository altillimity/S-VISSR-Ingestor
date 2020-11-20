#pragma once

#include <vector>
#include <array>
#include <cstdint>

class VISSRDeframer
{
private:
    // Main shifter
    uint64_t shifter;
    int goodBits;
    // Small function to push a bit into the frame
    void pushBit(uint8_t bit);
    // Framing variables
    uint8_t byteBuffer;
    bool writeFrame;
    int wroteBits, outputBits;
    std::vector<uint8_t> frameBuffer;

public:
    VISSRDeframer();
    std::vector<std::vector<uint8_t>> work(uint8_t *data, int len);
};