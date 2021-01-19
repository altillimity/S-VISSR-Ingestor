#include "decoder.h"
#include <iostream>
#include <filesystem>
#include "config.h"

std::string getFYFilename(std::tm *timeReadable, int channel)
{
    std::string utc_filename = ingestorConfig.sat_name + "_" + std::to_string(channel) + "_" +                                                              // Satellite name and channel
                               std::to_string(timeReadable->tm_year + 1900) +                                                                               // Year yyyy
                               (timeReadable->tm_mon + 1 > 9 ? std::to_string(timeReadable->tm_mon + 1) : "0" + std::to_string(timeReadable->tm_mon + 1)) + // Month MM
                               (timeReadable->tm_mday > 9 ? std::to_string(timeReadable->tm_mday) : "0" + std::to_string(timeReadable->tm_mday)) + "T" +    // Day dd
                               (timeReadable->tm_hour > 9 ? std::to_string(timeReadable->tm_hour) : "0" + std::to_string(timeReadable->tm_hour)) +          // Hour HH
                               (timeReadable->tm_min > 9 ? std::to_string(timeReadable->tm_min) : "0" + std::to_string(timeReadable->tm_min)) +             // Minutes mm
                               (timeReadable->tm_sec > 9 ? std::to_string(timeReadable->tm_sec) : "0" + std::to_string(timeReadable->tm_sec)) + "Z";        // Seconds ss
    return utc_filename;
}

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

    std::string disk_folder = output_folder + "/SVISSR_" + timestamp;

    std::cout << "Channel 1... " + getFYFilename(timeReadable, 1) + ".png"  << std::endl;
    image1.save_png(std::string(disk_folder + "/" + getFYFilename(timeReadable, 1) + ".png").c_str());

    std::cout << "Channel 2... " + getFYFilename(timeReadable, 2) + ".png"  << std::endl;
    image2.save_png(std::string(disk_folder + "/" + getFYFilename(timeReadable, 2) + ".png").c_str());

    std::cout << "Channel 3... " + getFYFilename(timeReadable, 3) + ".png"  << std::endl;
    image3.save_png(std::string(disk_folder + "/" + getFYFilename(timeReadable, 3) + ".png").c_str());

    std::cout << "Channel 4... " + getFYFilename(timeReadable, 4) + ".png"  << std::endl;
    image4.save_png(std::string(disk_folder + "/" + getFYFilename(timeReadable, 4) + ".png").c_str());

    std::cout << "Channel 5... " + getFYFilename(timeReadable, 5) + ".png"  << std::endl;
    image5.save_png(std::string(disk_folder + "/" + getFYFilename(timeReadable, 5) + ".png").c_str());

    writingImage = false;
}

SVISSRDecoder::SVISSRDecoder(std::string out)
{
    // Counters and so
    writingImage = false;
    endCount = 0;
    nonEndCount = 0;
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

        // Parse counter
        int counter = frameBuffer[67] << 8 | frameBuffer[68];

        // Safeguard
        if (counter > 2500)
            return;

        // Parse scan status
        int status = frameBuffer[3] % (int)pow(2, 2); // Decoder scan status

        // We only want forward scan data
        if (status != 3)
            return;

        //std::cout << counter << std::endl;

        // Try to detect a new scan
        // This is not the best way, but it works...
        if (counter > 2490 && counter <= 2500)
        {
            endCount++;
            nonEndCount = 0;

            if (endCount > 5)
            {
                endCount = 0;
                std::cout << "Full disk end detected!" << std::endl;

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
            nonEndCount++;
            if (endCount > 0)
                endCount -= 1;
        }

        // Process it
        vissrImageReader.pushFrame(frameBuffer);

        // Show very basic progress
        if (!writingImage)
            std::cout << "\rApproximate full disk progress : " << round(((float)counter / 2500.0f) * 1000.0f) / 10.0f << "%     ";
    }

    vissrFrames.clear();
}

void SVISSRDecoder::forceWriteFullDisks()
{
    if (writingImage = true)
        return;

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