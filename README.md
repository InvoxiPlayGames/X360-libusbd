# X360-libusbd

Attempting to use [libusbd](https://github.com/shinyquagsire23/libusbd) library to emulate an Xbox 360 controller using a computer.

This is a research project for now, this is not intended for any sort of usage (yet!). Currently it just spams all the inputs.

Only tested on macOS Monterey 12.5 on a MacBook Pro 14" (2021) - may work under Linux on other machines, too, I don't know.

To build, compile libusbd and copy the dylib/so to this folder, then run `make`.

## TODO:

- Actual input from somewhere
- Device subtypes
- XSM3 (may need extra work on libusbd's behalf)