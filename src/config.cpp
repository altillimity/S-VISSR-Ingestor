#include <yaml-cpp/yaml.h>
#include "config.h"
#include <iostream>

IngestorConfig ingestorConfig;

void loadConfig()
{
    YAML::Node configFile;
    try
    {
        configFile = YAML::LoadFile("config.yml");
    }
    catch (YAML::Exception &e)
    {
        std::cout << e.what() << std::endl;
        exit(1);
    }

    ingestorConfig.frequency = configFile["radio"]["frequency"].as<long>();
    ingestorConfig.samplerate = configFile["radio"]["sample_rate"].as<long>();
    ingestorConfig.gain = configFile["radio"]["gain"].as<int>();
    ingestorConfig.bias = configFile["radio"]["biastee"].as<bool>();
    ingestorConfig.device = configFile["radio"]["device"].as<std::string>();
    ingestorConfig.data_directory = configFile["data_directory"].as<std::string>();
    ingestorConfig.write_demod_bin = configFile["write_demod_bin"].as<bool>();
    ingestorConfig.sat_name = configFile["satellite_name"].as<std::string>();
}
