# qo-100-tools
Various tools for transmission and reception on the QO-100 sattelite bands. Includes everything I need (hopefuully) to build a fully remote managed RF frontend to be placed on the roof, near the antenna while the only thing coming to the shack would be power, two coax cables (TX IF and RX IF) and one UTP ethernet cable.

## Included tools
 - Upconverter - Converts HF to the uplink frequency of the sattelite (2.4 GHz)
 - PA - Boosts the output power to 20 W (v1) and 250 W (v2) maximum
 - GPSDO - GPS Disciplined Oscillator to generate different precise reference frequencies
 - LNB Controller - LNB Bias and reference injector controller
 - Digital Power Meter - Module to measure DC power, used to calculate the PA efficiency
 - Digital RF Power Meter - Module to measure RF power, used to calculate the PA efficiency
 - Generic Filter - Footprints to build up to 11th order Butterworth filters
 - Orange Pi Dock - Main board to house an OPi to control the whole system
 - Relay Controller - Module to provide relay coil switching capabilities
 - RF Switch - Implements a 5PST or 4PST RF switch for routing purposes

## Dependencies
 - [icestorm](https://github.com/cliffordwolf/icestorm) - FPGA toolchain
 - [arm-none-eabi-gcc](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads) - MCUs toolchain
 - [Device CMSIS](https://www.keil.com/dd2/) - CMSIS headers defining the MCU memories, peripherals, etc...
 - [Core CMSIS](https://github.com/ARM-software/CMSIS_5) - CMSIS headers defining the ARM Cores
 - [armmem](https://github.com/vankxr/armmem) - ELF file analyzer (required for the MCU Makefiles to work)

## Authors

* **João Silva** - [vankxr](https://github.com/vankxr)

## License

The content of this repository is licensed under the [GNU General Public License v3.0](LICENSE).
