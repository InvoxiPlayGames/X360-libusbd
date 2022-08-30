# X360-libusbd

Attempting to use [libusbd](https://github.com/shinyquagsire23/libusbd) library to emulate an Xbox 360 controller using a computer.

This is a research project for now, this is not intended for any sort of usage (yet!). Currently it just spams all the inputs.

Only tested on macOS Monterey 12.5 on a MacBook Pro 14" (2021) - once libusbd is implemented for Linux fully, it should work there, too.

To build, compile libusbd and copy the dylib/so to this folder, then run `make`. (Requires a version of libusbd with `libusbd_iface_set_description`)

## TODO:

- Actual input from somewhere
- XSM3 - currently sends init packet, does nothing afterwards.