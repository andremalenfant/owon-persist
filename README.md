# An ESP32-C3 add-on to the OWON XDM1041 Multi-meter to add the persistence of configured settings.

## Inpired by the work of:
  - TheHWCave for it's great documentation that is much better than owon's (https://github.com/TheHWcave/OWON-XDM1041/tree/main)
  - jantman (https://github.com/jantman/owon-xdm1041-server)
  - Elektroarzt (https://github.com/Elektroarzt/owon-xdm-remote)

jantman and Elektroarzt are focused on remote data acquisition, where I am just aiming at settings persistence. Also, my implementation is using ESP-IDF instead of MicroPython (more used to it and the MicroPython dev environment has been unstable for me).

One of the annoying this about the multi-meter is it does not store configurations. Each time it is powered up, defaults are set. This module polls the multi-meter each seconds or so per available setting and stores them to the ESP's nvram.

On power-up, cold or warm, these settings are then pushed back to the multi-meter.

It covers function, speed, auto range and manual ranges for Volt/Amp/Res/Cap. The frequency ranges and temperature probe type are not yet implemented.

The code might seem complicated at first, but the protocol implementation is quite inconsistent and it requires some gymnastics to avoid ugly branching everywhere.

## Installation

  - Build and flash with platformio
  - Connect RX and TX pin and PWR following the wiring in Elektroarzt (https://github.com/Elektroarzt/owon-xdm-remote)
  - Included is a stl of a bracket to replace the OWON USB module. Just hot glue the ESP32-C3 to the flat and screw in the back of the multi-meter.
