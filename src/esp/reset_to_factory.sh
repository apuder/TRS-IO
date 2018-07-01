#!/bin/sh

# The following command will erase the OTA data partition thus resetting
# the ESP32 to the factory image

esptool.py --chip esp32 --port /dev/ttyS11 --baud 115200 erase_region 0xd000 0x2000

