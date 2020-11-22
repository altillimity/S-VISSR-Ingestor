#pragma once

#include <string>
#include "ctpl/ctpl_stl.h"
#include "processing/svissr_reader.h"
#include "processing/derand.h"
#include "processing/deframer.h"

class SVISSRDecoder
{
private:
    // Processing buffer
    uint8_t *frameBuffer;

    // Utils values
    bool writingImage;
    int zeroCount;
    int nonZeronCount;
    int lastNonZero;

    // Image readers
    SVISSRReader vissrImageReader;

    // PseudoNoise (PN) derandomizer
    PNDerandomizer derandomizer;

    // Deframer
    VISSRDeframer svissrDeframer;
    std::vector<std::vector<uint8_t>> vissrFrames;

    // Images used as a buffer when writing it out
    cimg_library::CImg<unsigned short> image1;
    cimg_library::CImg<unsigned short> image2;
    cimg_library::CImg<unsigned short> image3;
    cimg_library::CImg<unsigned short> image4;
    cimg_library::CImg<unsigned short> image5;

    // Saving is multithreaded
    std::shared_ptr<ctpl::thread_pool> imageSavingThreadPool;
    std::future<void> imageSavingFuture;

    // Output folder
    std::string output_folder;

private:
    void writeFullDisks();

public:
    SVISSRDecoder(std::string out);
    ~SVISSRDecoder();
    void processBuffer(uint8_t *buffer, int size);
    void forceWriteFullDisks();
};