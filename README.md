# S-VISSR (FengYun-2x) Decoder

A program to decode the S-VISSR imager transmission from FY-2x satellites  
Currently, as I am writing this, FY-2H and FY-2G are active.

The frequency is 1687.5Mhz.

***Huge thanks to @MartanBlaho and @ZSzstanga for providing testing and data to work with!***

**This software is still in beta, so bugs and potential instabilities are to be expected**

# Usage

The program can either run as a 24/7 service for automated station setups or as a standalone "one-time" decoder (processing a single .bin).

## Standalone

First of all, record a baseband at 2MSPS or more and demodulate with the included FY2Demod.grc (GNU Radio 3.8), obtaining a .bin.   
Then, run the .bin through the decoder as follow :
``` 
SVISSR-Ingestor --dec gvar.bin
```

## Autonomous

In autonomous mode, the ingestor demodulates data from a SDR source, decodes it live and output images automatically. This most probably is the easiest way to go with even if you're not operating a 24/7 automated setup.   
This should work just fine on a Raspberry PI 4.

### Configuration

You will first need to create a config.yml in your work folder (where the binary is or will be executed), based on the template included in the repo.

```
# SVISSR-Ingestor configuration

# SDR Settings
radio:
  device: "driver=airspy" # SoapySDR device string
  frequency: 1687500000 # S-VISSR Frequency
  sample_rate: 3000000 # Samplerate. Has to be >= 0.660MSPS
  gain: 40 # SDR Gain, an integer
  biastee: true

# Data director to save the pictures in
data_directory: SVISSR_DATA

# Write demodulated data to svissr.bin (debugging only, ignore otherwise)
write_demod_bin: false
```

You will have to configure it accordingly for your setup / station. This default template is suitable for an Airspy-based setup, and should not require any modification for quick testing run apart from the gain.

Automated stations will likely want to set the data directory to be an absolute path.

### Running

Running the software is done with no argument, in which case it will default to demod + decode :
```
SVISSR-Ingestor
```

If you're installing it on a fulltime operating station, you will want to start it inside a screen to keep it running when logging off, or create a systemd unit.   
Eg,
```
sudo apt install screen # Install the program on debian-based systems
screen -S fy2 SVISSR-Ingestor # Create a screen, press CTRL + A / D to detach
screen -x fy2 # Attach to the screen
```

# Building

This will require [libpng](https://github.com/glennrp/libpng), [yamlcpp](https://github.com/jbeder/yaml-cpp), [SoapySDR](https://github.com/pothosware/SoapySDR) and [libdsp](https://github.com/altillimity/libdsp).   

If you are using a Debian-based system (Ubuntu, Raspberry PI, etc), you can just run those commands

```
sudo apt -y install libyamlcpp-dev libpng-dev libsoapysdr-dev cmake build-essential
git clone https://github.com/altillimity/libdsp.git
cd libdsp
mkdir build && cd build
cmake ..
make -j2
sudo make install
cd ../..
git clone https://github.com/altillimity/S-VISSR-Ingestor.git
cd S-VISSR-Ingestor
mkdir build && cd build
cmake ..
make -j2
sudo make install
```

Windows binaries are available in the repository with support for the following SDRs :
- Airspy
- RTL-SDR
   
If you need support for something else on Windows, feel free to open an issue and I will add it.