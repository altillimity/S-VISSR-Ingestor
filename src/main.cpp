#include <iostream>
#include <fstream>
#include <complex>
#include <vector>
#include "derand.h"
#include "svissr_reader.h"
#include "deframer.h"
#include <thread>
#include <filesystem>

// Reader and full disk counter
SVISSRReader channelVIS5Reader;
int fulldisk = 0;

// Function to save images
void saveImages()
{
    std::cout << std::endl;
    std::cout << "Writing images... (Can take a while)" << std::endl;

    std::cout << "IR 1..." << std::endl;
    channelVIS5Reader.getImageIR1().save_png(std::string(std::to_string(fulldisk) + "IR-1.png").c_str());

    std::cout << "IR 2..." << std::endl;
    channelVIS5Reader.getImageIR2().save_png(std::string(std::to_string(fulldisk) + "IR-2.png").c_str());

    std::cout << "IR 3..." << std::endl;
    channelVIS5Reader.getImageIR3().save_png(std::string(std::to_string(fulldisk) + "IR-3.png").c_str());

    std::cout << "IR 4..." << std::endl;
    channelVIS5Reader.getImageIR4().save_png(std::string(std::to_string(fulldisk) + "IR-4.png").c_str());

    std::cout << "Visible..." << std::endl;
    channelVIS5Reader.getImageVIS().save_png(std::string(std::to_string(fulldisk) + "VIS.png").c_str());

    fulldisk++;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cout << "Usage : " << argv[0] << " inputDemodDiff.bin" << std::endl;
        return 0;
    }

    // Output and Input file
    std::ifstream data_in(argv[1], std::ios::binary);

    if (!data_in)
    {
        std::cout << "Could not open input file!" << std::endl;
        return 1;
    }

    // Read buffer
    uint8_t buffer[1025];

    // VISSR Deframer
    VISSRDeframer gvarDeframer;

    // Graphics
    std::cout << "---------------------------" << std::endl;
    std::cout << " S-VISSR Decoder by Aang23" << std::endl;
    std::cout << "---------------------------" << std::endl;
    std::cout << std::endl;

    // Derandomizer
    PNDerandomizer derandomizer;

    // Another frame buffer
    uint8_t *frameBuffer = new uint8_t[354848];

    int zeroCount = 0;
    int nonZeronCount = 0;
    int lastNonZero = 0;

    // Read until EOF
    while (!data_in.eof())
    {
        // Read buffer
        data_in.read((char *)buffer, 1024);

        // Extract content, deframe, and push into decoder
        std::vector<std::vector<uint8_t>> frame_array = gvarDeframer.work(buffer, 1024);

        for (std::vector<uint8_t> &frame : frame_array)
        {
            // Copy to our frame buffer
            std::memcpy(frameBuffer, &frame[0], frame.size());

            // Derandomize this frame
            derandomizer.derandData(frameBuffer, 354848 / 8);

            // Process it
            channelVIS5Reader.pushFrame(frameBuffer);

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
                    saveImages();
                    std::cout << std::endl;
                    channelVIS5Reader.reset();
                }
            }
            else
            {
                nonZeronCount++;
                zeroCount = 0;
            }

            // Show very basic progress
            std::cout << "\r(Very) Approximate full disk progress : " << round(((float)counter / 2500.0f) * 1000.0f) / 10.0f << "%     ";
        }
    }

    std::cout << std::endl;

    // Save what's left
    saveImages();

    data_in.close();
}