# S-VISSR (FengYun-2x) Decoder

A program to decode the S-VISSR imager transmission from FY-2x satellites  
Currently, as I am writing this, FY-2H and FY-2G are active.

**This software is still in beta, so bugs and potential instabilities are to be expected**

# Usage

Currently, no live processing is implemented, even if it will be later on to allow fully autonomous setups.

The current process currently requires demodulating with FY2Demod.grc (GNU Radio 3.8), which can either be done live or by recording baseband and then processing. The obtained .bin is then ran through the decoder.   

`./SVISSR-Decoder file.bin`

# Building

This will require libpng, which on debian systems such as Ubuntu can be installed with `sudo apt install libpng-dev`.   
Then, standard CMake building applies :   

```
git clone https://github.com/altillimity/S-VISSR-Decoder.git
cd S-VISSR-Decoder
mkdir build && cd build
cmake ..
make -j2
sudo make install
```