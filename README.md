PS/2 keyboar converter for Raspberry Pi PICO RP2040
===================================================
2022-12-14

This was written for me to learn RP2040, pico-sdk and tinyusb. Not tested well.


Pin configuration
-----------------
5V-3.3V voltage shifter is required on the GPIO pins.

    #define CLOCK_PIN   2
    #define DATA_PIN    3


Key mapping
-----------
This array defines mapping Code Set 2 to HID usage.

    const uint16_t cs2_to_hid[] = {

Its content is uint16_t value comprised of (Usage page << 12 | Usage ID) where:

    Usage page: 4-bit       0x0(Keyboard by default), 0x7(Keyboard), 0xC(Consumer), 0x1(Generic Desktiop/System Control)
    Usage ID:   12-bit


TODO
----
- Refine Descriptors: NKRO, IAD
- System usage page
- NKRO support
- LED indicators
