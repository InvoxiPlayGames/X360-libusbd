#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "libusbd.h"

volatile sig_atomic_t stop;

void inthand(int signum) {
    stop = 1;
}

int control_class_impl(libusbd_setup_callback_info_t* info) {
    printf("[CONTROL] bmRequestType:%02x bRequest:%02x wIndex:%04x wLength:%04x wValue:%04x\n", info->bmRequestType, info->bRequest, info->wIndex, info->wLength, info->wValue);
    return 0;
}

int security_class_impl(libusbd_setup_callback_info_t* info) {
    printf("[SECURITY] bmRequestType:%02x bRequest:%02x wIndex:%04x wLength:%04x wValue:%04x\n", info->bmRequestType, info->bRequest, info->wIndex, info->wLength, info->wValue);
    return 0;
}

void hexdump(uint8_t *buf, int len) {
    for (int i = 0; i < len; i++) {
        if (i != 0 && i % 0x10 == 0x0) printf("\n");
        printf("%02X ", buf[i]);
    }
    printf("\n");
}

#define X360_IF_CONTROL  0
#define X360_IF_HEADSET  1
#define X360_IF_UNKNOWN  2
#define X360_IF_SECURITY 3

typedef struct _xinput_report {
    uint8_t message_type;
    uint8_t message_size;
} __attribute__((packed)) xinput_report;

typedef struct _xinput_report_controls {
    xinput_report header;
    uint8_t buttons1;
    uint8_t buttons2;
    uint8_t left_trigger;
    uint8_t right_trigger;
    int16_t left_stick_x;
    int16_t left_stick_y;
    int16_t right_stick_x;
    int16_t right_stick_y;
    uint8_t padding[6];
} __attribute__((packed)) xinput_report_controls;

#define CONTROLLER_RATE_MS 4

int main()
{
    // our libusbd context
    libusbd_ctx_t* pCtx;
    // control endpoints (interface 0)
    uint64_t control_ep_out;
    uint64_t control_ep_in;
    // audio/ext endpoints (interface 1)
    uint64_t microphone_ep_out;
    uint64_t headset_ep_in;
    uint64_t expansion_ep_out;
    uint64_t expansion_ep_in;
    // unknown endpoints (interface 2)
    uint64_t unknown_ep_out;
    // XSM3 has no endpoints.

    // register a handler for interrupts so we can safely close libusbd
    signal(SIGINT, inthand);

    // --- DEVICE CONFIGURATION ---
    // start up libusbd and register as xbox 360 controller
    libusbd_init(&pCtx);
    libusbd_set_vid(pCtx, 0x045E); // ms corp
    libusbd_set_pid(pCtx, 0x028E); // wired controller
    libusbd_set_version(pCtx, 0x0114); // version
    libusbd_set_class(pCtx, 0xFF);
    libusbd_set_subclass(pCtx, 0xFF);
    libusbd_set_protocol(pCtx, 0xFF);
    // set strings. TOOD: how xsm3 lol
    libusbd_set_manufacturer_str(pCtx, "REAL Xbonx");
    libusbd_set_product_str(pCtx, "Controller");
    libusbd_set_serial_str(pCtx, "Emma");
    // allocate 4 interfaces and then finalise the device configuration descriptor
    uint8_t iface_num = 0;
    libusbd_iface_alloc(pCtx, &iface_num);
    libusbd_iface_alloc(pCtx, &iface_num);
    libusbd_iface_alloc(pCtx, &iface_num);
    libusbd_iface_alloc(pCtx, &iface_num);
    libusbd_config_finalize(pCtx);

    // -- INTERFACE 0: CONTROL DATA --
    // set up the classes
    libusbd_iface_set_class(pCtx, X360_IF_CONTROL, 0xFF);
    libusbd_iface_set_subclass(pCtx, X360_IF_CONTROL, 0x5D);
    libusbd_iface_set_protocol(pCtx, X360_IF_CONTROL, 0x1);
    // set the extra descriptor. idk what this does.
    uint8_t control_report_desc[] = { 0x11, 0x21, 0x00, 0x01, 0x01, 0x25, 0x81, 0x14, 0x00, 0x00, 0x00, 0x00, 0x13, 0x01, 0x08, 0x00, 0x00 };
    libusbd_iface_standard_desc(pCtx, X360_IF_CONTROL, 0x21, 0xF, control_report_desc, sizeof(control_report_desc));
    // add 2 endpoints, one for input, one for output
    libusbd_iface_add_endpoint(pCtx, X360_IF_CONTROL, USB_EPATTR_TTYPE_INTR, USB_EP_DIR_IN, 32, 4, 0, &control_ep_out);
    libusbd_iface_add_endpoint(pCtx, X360_IF_CONTROL, USB_EPATTR_TTYPE_INTR, USB_EP_DIR_OUT, 32, 8, 0, &control_ep_in);
    // set up a callback for class commands
    libusbd_iface_set_class_cmd_callback(pCtx, X360_IF_CONTROL, control_class_impl);
    // finalise the control data interface
    libusbd_iface_finalize(pCtx, X360_IF_CONTROL);

    // -- INTERFACE 1: HEADSET DATA --
    // set up the classes
    libusbd_iface_set_class(pCtx, X360_IF_HEADSET, 0xFF);
    libusbd_iface_set_subclass(pCtx, X360_IF_HEADSET, 0x5D);
    libusbd_iface_set_protocol(pCtx, X360_IF_HEADSET, 0x3);
    // set the extra descriptor. idk what this does.
    uint8_t headset_desc[] = { 0x1B, 0x21, 0x00, 0x01, 0x01, 0x01, 0x82, 0x40, 0x01, 0x02, 0x20, 0x16, 0x83, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x16, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    libusbd_iface_standard_desc(pCtx, X360_IF_HEADSET, 0x21, 0xF, headset_desc, sizeof(headset_desc));
    // add the endpoints for headset in-out
    libusbd_iface_add_endpoint(pCtx, X360_IF_HEADSET, USB_EPATTR_TTYPE_INTR, USB_EP_DIR_IN, 32, 2, 0, &microphone_ep_out);
    libusbd_iface_add_endpoint(pCtx, X360_IF_HEADSET, USB_EPATTR_TTYPE_INTR, USB_EP_DIR_OUT, 32, 4, 0, &headset_ep_in);
    // and the unknown endpoints
    libusbd_iface_add_endpoint(pCtx, X360_IF_HEADSET, USB_EPATTR_TTYPE_INTR, USB_EP_DIR_IN, 32, 64, 0, &expansion_ep_out);
    libusbd_iface_add_endpoint(pCtx, X360_IF_HEADSET, USB_EPATTR_TTYPE_INTR, USB_EP_DIR_OUT, 32, 16, 0, &expansion_ep_in);
    // finalise the headset data interface
    libusbd_iface_finalize(pCtx, X360_IF_HEADSET);

    // -- INTERFACE 2: UNKNOWN --
    // set up the classes
    libusbd_iface_set_class(pCtx, X360_IF_UNKNOWN, 0xFF);
    libusbd_iface_set_subclass(pCtx, X360_IF_UNKNOWN, 0x5D);
    libusbd_iface_set_protocol(pCtx, X360_IF_UNKNOWN, 0x2);
    // set the extra descriptor. idk what this does.
    uint8_t unknown_desc[] = { 0x09, 0x21, 0x00, 0x01, 0x01, 0x22, 0x84, 0x07, 0x00 };
    libusbd_iface_standard_desc(pCtx, X360_IF_UNKNOWN, 0x21, 0xF, unknown_desc, sizeof(unknown_desc));
    // add the lone unknown endpoint
    libusbd_iface_add_endpoint(pCtx, X360_IF_UNKNOWN, USB_EPATTR_TTYPE_INTR, USB_EP_DIR_IN, 32, 16, 0, &unknown_ep_out);
    // finalise the unknown data interface
    libusbd_iface_finalize(pCtx, X360_IF_UNKNOWN);

    // -- INTERFACE 3: SECURITY --
    // TODO: this doesn't work! to get a console to talk, iInterface must go to a string descriptor (retail controller has it at 0x04) that says
    // Xbox Security Method 3, Version 1.00, ©️ 2005 Microsoft Corporation. All rights reserved.
    // after that, figure out how to answer. see oct0xor's research: http://oct0xor.github.io/2017/05/03/xsm3/
    // set up the classes
    libusbd_iface_set_class(pCtx, X360_IF_SECURITY, 0xFF);
    libusbd_iface_set_subclass(pCtx, X360_IF_SECURITY, 0xFD);
    libusbd_iface_set_protocol(pCtx, X360_IF_SECURITY, 0x13);
    // set the extra descriptor. idk what this does.
    uint8_t security_desc[] = { 0x06, 0x41, 0x00, 0x01, 0x01, 0x03 };
    libusbd_iface_standard_desc(pCtx, X360_IF_SECURITY, 0x41, 0xF, security_desc, sizeof(security_desc));
    // security has no endpoints, but it uses class commands to interact
    libusbd_iface_set_class_cmd_callback(pCtx, X360_IF_SECURITY, security_class_impl);
    // finalise the security data interface
    libusbd_iface_finalize(pCtx, X360_IF_SECURITY);

    int16_t rotate_cycle = 0;
    xinput_report_controls controls = { 0 };
    controls.header.message_type = 0x00; // input report
    controls.header.message_size = sizeof(controls);

    printf("ready!\n");
    while (!stop)
    {
        int32_t s_ret;
        uint8_t control_read[32];

        // read if there's been any messages
        s_ret = libusbd_ep_read(pCtx, X360_IF_CONTROL, control_ep_in, control_read, sizeof(control_read), CONTROLLER_RATE_MS);
        // the other end hasn't enumerated the device
        if (s_ret == LIBUSBD_NOT_ENUMERATED) {
            printf("waiting for device enumeration...\n");
            sleep(1);
            continue;
        } else if (s_ret == LIBUSBD_TIMEOUT) {
            printf("read timeout\n");
            sleep(1);
            continue;
        } else if (s_ret < 0) {
            printf("unknown read error %x\n", s_ret);
            sleep(1);
            continue;
        } else if (s_ret > 0) {
            printf("read packet: %x\n", s_ret);
            hexdump(control_read, s_ret);
        }

        // if that didn't fail, we can send our control data
        s_ret = libusbd_ep_write(pCtx, X360_IF_CONTROL, control_ep_out, &controls, sizeof(controls), CONTROLLER_RATE_MS);
        if (s_ret == LIBUSBD_TIMEOUT) {
            printf("write timed out\n");
            sleep(1);
            continue;
        } else if (s_ret < 0) {
            printf("unknown write error %x\n", s_ret);
            sleep(1);
            continue;
        } else {
            //printf("sent %x bytes\n", s_ret);
        }

        // change the stick positions
        controls.left_stick_x = rotate_cycle;
        controls.left_stick_y = -rotate_cycle;
        controls.right_stick_x = -rotate_cycle;
        controls.right_stick_y = rotate_cycle;
        // set some random values
        controls.left_trigger = rand() & 0xFF;
        controls.right_trigger = rand() & 0xFF;
        controls.buttons1 = rand() & 0xFF;
        controls.buttons2 = rand() & 0xF3; // don't press guide. steam fucking sucks.
        // boogie woogie
        rotate_cycle += 1000;
    }

    libusbd_free(pCtx);

    printf("asd\n");
}