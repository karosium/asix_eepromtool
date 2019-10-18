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

How?
----

* install libusb-1.0-0-dev (or your distro's equivalent package)
* gcc asix_eepromtool.c -o asix_eepromtool -I/usr/include/libusb-1.0 -lusb-1.0
* sudo ./asix_eepromtool
