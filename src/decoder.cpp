#include "decoder.h"
#include <iostream>
#include <filesystem>

void SVISSRDecoder::writeFullDisks()
{
    const time_t timevalue = time(0);
    std::tm *timeReadable = gmtime(&timevalue);
    std::string timestamp = std::to_string(timeReadable->tm_year + 1900) + "-" +
                            std::to_string(timeReadable->tm_mon + 1) + "-" +
                            std::to_string(timeReadable->tm_mday) + "_" +
                            std::to_string(timeReadable->tm_hour) + "-" +
                            (timeReadable->tm_min > 9 ? std::to_string(timeReadable->tm_min) : "0" + std::to_string(timeReadable->tm_min));

    std::cout << "Full disk finished, saving at SVISSR_" + timestamp + "..." << std::endl;

    if (!std::filesystem::exists(output_folder + "/SVISSR_" + timestamp))
    {
        std::filesystem::create_directory(output_folder + "/SVISSR_" + timestamp);
    }
    else
    {
        while (std::filesystem::exists(output_folder + "/SVISSR_" + timestamp))
        {
            timestamp += "-bis";
        }
        std::filesystem::create_directory(output_folder + "/SVISSR_" + timestamp);
    }

    std::cout << "Channel 1..." << std::endl;
    image1.save_png(std::string(output_folder + "/SVISSR_" + timestamp + "/IR-1.png").c_str());

    std::cout << "Channel 2..." << std::endl;
    image2.save_png(std::string(output_folder + "/SVISSR_" + timestamp + "/IR-2.png").c_str());

    std::cout << "Channel 3..." << std::endl;
    image3.save_png(std::string(output_folder + "/SVISSR_" + timestamp + "/IR-3.png").c_str());

    std::cout << "Channel 4..." << std::endl;
    image4.save_png(std::string(output_folder + "/SVISSR_" + timestamp + "/IR-4.png").c_str());

    std::cout << "Channel 5..." << std::endl;
    image5.save_png(std::string(output_folder + "/SVISSR_" + timestamp + "/VIS.png").c_str());

    writingImage = false;
}

SVISSRDecoder::SVISSRDecoder(std::string out)
{
    // Counters and so
    writingImage = false;
    zeroCount = 0;
    nonZeronCount = 0;
    lastNonZero = 0;

    // Init thread pool
    imageSavingThreadPool = std::make_shared<ctpl::thread_pool>(1);

    // Init reader
    vissrImageReader.reset();

    // Processing buffer
    frameBuffer = new uint8_t[354848];

    // Init save folder
    output_folder = out;
    std::filesystem::create_directory(output_folder);
}

SVISSRDecoder::~SVISSRDecoder()
{
    delete[] frameBuffer;
}

void SVISSRDecoder::processBuffer(uint8_t *buffer, int size)
{
    // Extract content, deframe, and push into decoder
    vissrFrames = svissrDeframer.work(buffer, size);

    for (std::vector<uint8_t> &frame : vissrFrames)
    {
        // Copy to our frame buffer
        std::memcpy(frameBuffer, &frame[0], frame.size());

        // Derandomize this frame
        derandomizer.derandData(frameBuffer, 354848 / 8);

        // Process it
        vissrImageReader.pushFrame(frameBuffer);

        // Parse counter
        int counter = frameBuffer[67] << 8 | frameBuffer[68];

        //std::cout << counter << std::endl;

        // Try to detect a new scan
        if (counter == 0)
        {
            zeroCount++;
            nonZeronCount = 0;

            if (zeroCount > 80)
            {
                zeroCount = 0;
                std::cout << "Full disk detected!" << std::endl;

                if (!writingImage)
                {
                    writingImage = true;

                    // Backup images
                    image1 = vissrImageReader.getImageIR1();
                    image2 = vissrImageReader.getImageIR2();
                    image3 = vissrImageReader.getImageIR3();
                    image4 = vissrImageReader.getImageIR4();
                    image5 = vissrImageReader.getImageVIS();

                    // Write those
                    if (imageSavingFuture.valid())
                        imageSavingFuture.get();
                    imageSavingThreadPool->push([&](int) { writeFullDisks(); });

                    // Reset readers
                    vissrImageReader.reset();
                }

                std::cout << std::endl;
            }
        }
        else
        {
            nonZeronCount++;
            zeroCount = 0;
        }

        // Show very basic progress
        if (!writingImage)
            std::cout << "\rApproximate full disk progress : " << round(((float)counter / 2500.0f) * 1000.0f) / 10.0f << "%     ";
    }

    vissrFrames.clear();
}

void SVISSRDecoder::forceWriteFullDisks()
{
    writingImage = true;

    // Backup images
    image1 = vissrImageReader.getImageIR1();
    image2 = vissrImageReader.getImageIR2();
    image3 = vissrImageReader.getImageIR3();
    image4 = vissrImageReader.getImageIR4();
    image5 = vissrImageReader.getImageVIS();

    // Write those
    if (imageSavingFuture.valid())
        imageSavingFuture.get();
    imageSavingThreadPool->push([&](int) { writeFullDisks(); });

    // Reset readers
    vissrImageReader.reset();
}