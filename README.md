# qo-100-tools
Various tools for transmission and reception on the QO-100 sattelite bands

## Included tools
 - Upconverter - Converts HF to the uplink frequency of the sattelite (2.4 GHz)
 - PA - Boosts the output power to 20 W maximum
 - GPSDO - GPS Disciplined Oscillator to generate different precise reference frequencies
 - LNB Controller - LNB Bias and reference injector controller

## Dependencies
 - [icestorm](https://github.com/cliffordwolf/icestorm) - FPGA toolchain
 - [arm-none-eabi-gcc](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads) - MCUs toolchain
 - [Device CMSIS](https://www.keil.com/dd2/) - CMSIS headers defining the MCU memories, peripherals, etc...
 - [Core CMSIS](https://github.com/ARM-software/CMSIS_5) - CMSIS headers defining the ARM Cores
 - [armmem](https://github.com/vankxr/armmem) - ELF file analyzer (required for the MCU Makefiles to work)

## Authors

* **João Silva** - [vankxr](https://github.com/vankxr)

## License

This firmware is licensed under [MIT](LICENSE).
