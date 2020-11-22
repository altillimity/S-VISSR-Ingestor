#include <iostream>
#include <fstream>
#include <complex>
#include <vector>
#include "decoder.h"
#include "config.h"
#include "demodulator.h"
#include "tclap/CmdLine.h"
#include <thread>
#include <filesystem>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>

int main(int argc, char *argv[])
{
    // Graphics
    std::cout << "---------------------------" << std::endl;
    std::cout << " S-VISSR Decoder by Aang23" << std::endl;
    std::cout << "---------------------------" << std::endl;
    std::cout << std::endl;

    TCLAP::CmdLine cmd("S-VISSR Ingestor by Aang23", ' ', "1.0");

    // File arguments
    TCLAP::ValueArg<std::string> optionOnlyDecoder("", "dec", "Only run the decoder, from .bin", false, "", "pdr.bin");
    TCLAP::ValueArg<std::string> optionBasebandDemod("", "bbdemod", "Demod from baseband (int16, 6MSPS for now)", false, "", "pdr.wav");
    TCLAP::ValueArg<std::string> optionBasebandDecode("", "bbdecode", "Demod and decode from baseband (int16, 6MSPS for now)", false, "", "pdr.wav");

    // Register all of the above options
    cmd.add(optionOnlyDecoder);
    cmd.add(optionBasebandDemod);
    cmd.add(optionBasebandDecode);

    // Parse
    try
    {
        cmd.parse(argc, argv);
    }
    catch (TCLAP::ArgException &e)
    {
        std::cout << e.error() << '\n';
        return 0;
    }

    // Only the decoder?
    if (optionOnlyDecoder.isSet())
    {
        std::cout << "Starting decoder only..." << std::endl;

        // File and read buffer
        std::ifstream inputFrameFile(optionOnlyDecoder.getValue(), std::ios::binary);
        uint8_t buffer[8192];

        // SVISSR Decoder
        SVISSRDecoder decoder(".");

        // Read until EOF
        while (!inputFrameFile.eof())
        {
            inputFrameFile.read((char *)buffer, 8192);

            decoder.processBuffer(buffer, 8192);
        }

        std::cout << "\nEnd of file reached" << std::endl;

        std::cout << "Writing remaining full disk data..." << std::endl;

        // Dump incomplete data if we've got any
        decoder.forceWriteFullDisks();

        // Close file
        inputFrameFile.close();
    }

    // Only the demod?
    else if (optionBasebandDemod.isSet())
    {
        std::cout << "Starting demodulator only..." << std::endl;

        // Files and buffers
        std::ifstream inputBaseband(optionBasebandDemod.getValue(), std::ios::binary);
        std::complex<float> buffer[8192];
        int16_t buffer_i16[8192 * 2];
        uint8_t buffer_out[8192];
        std::ofstream outputBin("svissr.bin", std::ios::binary);

        // SVISSR Demodulator
        SVISSRDemodulator demodulator(4e6, true);

        // Write thread
        bool shouldRun = true;
        std::thread writeThread([&]() {
            while (shouldRun)
            {
                int bytesOut = demodulator.pullMultiThread(buffer_out);
                outputBin.write((char *)buffer_out, bytesOut);
            }
        });

        // Read until EOF
        while (!inputBaseband.eof())
        {
            inputBaseband.read((char *)buffer_i16, 8192 * sizeof(int16_t) * 2);
            for (int i = 0; i < 8192; i++)
            {
                using namespace std::complex_literals;
                buffer[i] = (float)buffer_i16[i * 2] + (float)buffer_i16[i * 2 + 1] * 1if;
            }

            //int bytesOut = demodulator.workSingleThread(buffer, 8192, buffer_out);
            demodulator.pushMultiThread(buffer, 8192);
        }

        std::cout << "\nEnd of file reached" << std::endl;

        // Properly exit
        shouldRun = false;
        demodulator.stop();
        if (writeThread.joinable())
            writeThread.join();

        // Close files
        inputBaseband.close();
        outputBin.close();
    }

    // Demod + decode?
    else if (optionBasebandDecode.isSet())
    {
        std::cout << "Starting demodulator and decoder from baseband..." << std::endl;

        // Files and buffers
        std::ifstream inputBaseband(optionBasebandDecode.getValue(), std::ios::binary);
        std::complex<float> buffer[8192];
        int16_t buffer_i16[8192 * 2];
        uint8_t buffer_out[8192];

        // SVISSR Demodulator and Decoder
        SVISSRDemodulator demodulator(4e6, true);
        SVISSRDecoder decoder(".");

        // Decoding thread
        bool shouldRun = true;
        std::thread decThread([&]() {
            while (shouldRun)
            {
                int bytesOut = demodulator.pullMultiThread(buffer_out);
                decoder.processBuffer(buffer_out, bytesOut);
            }
        });

        // Read until EOF
        while (!inputBaseband.eof())
        {
            inputBaseband.read((char *)buffer_i16, 8192 * sizeof(int16_t) * 2);
            for (int i = 0; i < 8192; i++)
            {
                using namespace std::complex_literals;
                buffer[i] = (float)buffer_i16[i * 2] + (float)buffer_i16[i * 2 + 1] * 1if;
            }

            demodulator.pushMultiThread(buffer, 8192);
        }

        std::cout << "\nEnd of file reached" << std::endl;

        // Exit the decoding thread properly
        shouldRun = false;
        demodulator.stop();
        if (decThread.joinable())
            decThread.join();

        std::cout << "Writing full disks..." << std::endl;

        // Wait 2s to give a little bit of time for things to exit and so
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Dump incomplete data if we've got any
        decoder.forceWriteFullDisks();

        std::cout << "Done!" << std::endl;

        // Close files
        inputBaseband.close();
    }

    // Do it from an SDR, live
    else
    {
        // Load config
        loadConfig();

        std::cout << "Starting live demodulator and decoder..." << std::endl;

        // Files and buffers
        std::complex<float> buffer[8192];
        uint8_t buffer_out[8192];
        std::ofstream outputBin;
        if (ingestorConfig.write_demod_bin)
            outputBin = std::ofstream("svissr.bin", std::ios::binary);

        // SVISSR Demodulator and Decoder
        SVISSRDemodulator demodulator(ingestorConfig.samplerate, true);
        SVISSRDecoder decoder(ingestorConfig.data_directory);

        // SDR Stuff
        SoapySDR::Device *device = SoapySDR::Device::make(ingestorConfig.device);
        device->writeSetting("biastee", ingestorConfig.bias ? "true" : "false");
        device->setFrequency(SOAPY_SDR_RX, 0, ingestorConfig.frequency);
        device->setSampleRate(SOAPY_SDR_RX, 0, ingestorConfig.samplerate);
        device->setGain(SOAPY_SDR_RX, 0, ingestorConfig.gain);
        SoapySDR::Stream *device_stream = device->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);
        device->activateStream(device_stream, 0, 0, 0);

        // SDR variables
        void *sdr_buffer_ptr[] = {buffer};
        int flags;
        long long time_ns;

        // Decoding thread
        bool shouldRun = true;
        std::thread decThread([&]() {
            while (shouldRun)
            {
                int bytesOut = demodulator.pullMultiThread(buffer_out);

                if (ingestorConfig.write_demod_bin)
                    outputBin.write((char *)buffer_out, bytesOut);

                decoder.processBuffer(buffer_out, bytesOut);
            }
        });

        // Read until EOF
        while (device_stream)
        {
            int read_samples = device->readStream(device_stream, sdr_buffer_ptr, 8192, flags, time_ns, 1e6);

            if (read_samples > 8192 || read_samples <= 0)
                continue;

            demodulator.pushMultiThread(buffer, read_samples);
        }

        std::cout << "\nSDR Aborted" << std::endl;

        // Exit the decoding thread properly
        shouldRun = false;
        demodulator.stop();
        if (decThread.joinable())
            decThread.join();

        std::cout << "Writing full disks..." << std::endl;

        // Wait 2s to give a little bit of time for things to exit and so
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Dump incomplete data if we've got any
        decoder.forceWriteFullDisks();

        std::cout << "Done!" << std::endl;

        // Close files and SDR
        device->closeStream(device_stream);

        if (ingestorConfig.write_demod_bin)
            outputBin.close();
    }

    std::cout << "Exiting..." << std::endl;
}