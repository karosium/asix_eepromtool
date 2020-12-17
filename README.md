asix_eepromtool
---------------

What?
-----

Read and write onboard config EEPROM on USB ethernet adapters based on ASIX chips.

* AX88772
* AX88772A
* AX88772B
* AX88178
* AX88172
* AX8817X

Tested on AX88772B but should work on the rest. No driver support needed.
                                                    
Why?
----

* Change VID/PID, MAC address, etc.

It's not changing?
------------------

 * It's been [confirmed](https://github.com/karosium/asix_eepromtool/issues/1) that some chips ignore VID/PID bytes in the eeprom completely. This is likely because ASIX chips have an eFuse block to set VID/PID/MAC permanently and some manufacturers use it to make their products more tamper-resistant. If you change both instances of the VID/PID bytes in the eeprom and the chip is ignoring them then it's probably one of those.


How?
----

* install libusb-1.0-0-dev (or your distro's equivalent package)
* gcc asix_eepromtool.c -o asix_eepromtool -I/usr/include/libusb-1.0 -lusb-1.0
* sudo ./asix_eepromtool
