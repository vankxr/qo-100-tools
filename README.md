# qo-100-tools
Various tools for transmission and reception on the QO-100 satellite amateur transponder bands. Includes everything I need (hopefully) to build a fully remote managed RF frontend to be placed on the roof, near the antenna while only bringing a few cables between it and the shack: power, two coax cables (TX IF and RX IF) and one UTP ethernet cable.

## Included tools
 - Upconverter - Converts the VHF/UHF IF signal to the uplink frequency of the satellite (2.4 GHz)
 - PA - Boosts the output power to 20 W (v1) and 250 W (v2) maximum
 - PA Driver - Amplifier to drive the main 250W PA
 - Directional Couplers - Experimental, manually constructed with simple scalpel cut traces on some Rogers RO4350B substrate
 - GPSDO - GPS Disciplined Oscillator to generate different precise reference frequencies
 - LNB Controller - LNB Bias and reference injection controller
 - Digital Power Meter - Module to measure DC power, used to calculate the PA efficiency
 - Digital RF Power Meter - Module to measure RF power, used to calculate the PA efficiency
 - Generic Filter - Footprints to build up to 11th order Butterworth/Chebyshev filters
 - Orange Pi Dock - Main board to house an OPi that will be the main brain of the system
 - Relay Controller - Module to provide relay coil switching capabilities
 - RF Switch - Implements a 5PST or 4PST RF switch for routing purposes
 - PSU Interface - Interface board for HP Common Slot Power supplies
 - SAW Filter adapter - Footprints for various SAW filter packages
 - PA Bias controller - Bias generator and monitor for the Power Amplifiers
 - PA TEC controller - Peltier cooler controller for the big PA
 - I2C Buffer - I2C bus signal buffer
 - Environment sensors - Sensor carrier board
 - LoRa Transceiver - SX1276 breakout for LoRa experiments


## Dependencies
 - [icyradio](https://github.com/vankxr/icyradio/tree/qo100) - IcyRadio Software Defined Radio to transmit and receive IF signals to the roof RF frontend
 - [icestorm](https://github.com/cliffordwolf/icestorm) - FPGA toolchain
 - [arm-none-eabi-gcc](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads) - MCUs toolchain
 - [Device CMSIS](https://www.keil.com/dd2/) - CMSIS headers defining the MCU memories, peripherals, etc...
 - [Core CMSIS](https://github.com/ARM-software/CMSIS_5) - CMSIS headers defining the ARM Cores
 - [armmem](https://github.com/vankxr/armmem) - ELF file analyzer (required for the MCU Makefiles to work)

## Authors

* **João Silva** - [vankxr](https://github.com/vankxr)

## Special thanks

* [JLCPCB](https://jlcpcb.com) for sponsoring most of the PCBs needed for this project.
* [raplin](https://github.com/raplin) for the work on [reverse engineering the HP power supplies](https://github.com/raplin/DPS-1200FB)

## License

The content of this repository is licensed under the [GNU General Public License v3.0](LICENSE).
