#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "libusbd.h"
#include "x360_controller.h"

#define X360_IF_CONTROL  0
#define X360_IF_HEADSET  1
#define X360_IF_UNKNOWN  2
#define X360_IF_SECURITY 3

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

volatile sig_atomic_t stop;

void inthand(int signum) {
    stop = 1;
}

void hexdump(uint8_t *buf, int len) {
    for (int i = 0; i < len; i++) {
        if (i != 0 && i % 0x10 == 0x0) printf("\n");
        printf("%02X ", buf[i]);
    }
    printf("\n");
}

int control_class_impl(libusbd_setup_callback_info_t* info) {
    printf("[CONTROL] bmRequestType:%02x bRequest:%02x wIndex:%04x wLength:%04x wValue:%04x\n", info->bmRequestType, info->bRequest, info->wIndex, info->wLength, info->wValue);
    return 0;
}

unsigned char xsm3_static_data_gamepad[] = {
    0x49, 0x4B, 0x00, 0x00, 0x17, 0x04, 0xE1, 0x11,
    0x54, 0x15, 0xED, 0x88, 0x55, 0x21, 0x01, 0x33,
    0x00, 0x00, 0x80, 0x02, 0x5E, 0x04, 0x8E, 0x02,
    0x03, 0x00, 0x01, 0x01, 0xC1
};
unsigned char xsm3_challenge_in_buffer[0x30] = { 0 };
unsigned char xsm3_challenge_out_buffer[0x30] = { 0 };

typedef enum _xsm3_state_enum {
    XSM3_STATE_NOT_INITIALISED,
    XSM3_STATE_DEVICE_INFO_SENT,
    XSM3_STATE_CHALLENGE_1,
    XSM3_STATE_CHALLENGE_2,
    XSM3_STATE_COMPLETE
} xsm3_state_enum;

int xsm3_state = XSM3_STATE_NOT_INITIALISED;

int security_class_impl(libusbd_setup_callback_info_t* info) {
    // if the console is requesting state, we can populate the challenge input buffer
    // for some reason iokit doesn't populate the out_data buffer with input data
    // until *after* the callback completes, for some reason... luckily the console sends no new data in
    // so we can just fetch it here... 
    if (info->bRequest == 0x86) {
        memcpy(xsm3_challenge_in_buffer, info->out_data, sizeof(xsm3_challenge_in_buffer));
        hexdump(xsm3_challenge_in_buffer, sizeof(xsm3_challenge_in_buffer));
        // TODO: calculate the challenge response
    }

    printf("[XSM3 - bmR:0x%02x bR:0x%02x wI:0x%04x wL:0x%04x wV:0x%04x] ", info->bmRequestType, info->bRequest, info->wIndex, info->wLength, info->wValue);

    // switch what we're doing depending on the request
    switch (info->bRequest) {
        // console requesting information from the console
        case 0x81:
            printf("device info requested");
            memcpy(info->out_data, xsm3_static_data_gamepad, info->wLength);
            info->out_len = info->wLength;
            xsm3_state = XSM3_STATE_DEVICE_INFO_SENT;
            break;

        // recieving challenge request data from the console
        case 0x82:
            printf("challenge 1 received");
            info->out_len = info->wLength;
            xsm3_state = XSM3_STATE_CHALLENGE_1;
            break;
        case 0x87:
            printf("challenge 2 received");
            info->out_len = info->wLength;
            xsm3_state = XSM3_STATE_CHALLENGE_2;
            break;
        // console requesting challenge response data
        case 0x83:
            printf("challenge response requested");
            memcpy(info->out_data, xsm3_challenge_out_buffer, info->wLength);
            info->out_len = info->wLength;
            break;

        // console telling controller that authentication is finished
        case 0x84:
            printf("authentication completed!");
            info->out_len = info->wLength;
            xsm3_state = XSM3_STATE_COMPLETE;
            break;
        // console requesting the current state from authentication
        case 0x86:
            printf("state requested");
            short state = 2; // completed. 1 = in-progress
            memcpy(info->out_data, &state, sizeof(short));
            info->out_len = sizeof(short);
            break;

        // what are you doing
        default:
            printf("!! XSM3 UNKNOWN PACKET !!");
            break;
    }
    printf("\n");
    return 0;
}

#define CONTROLLER_RATE_MS 4

int main()
{
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
    // only now can we finalise the configuration
    libusbd_config_finalize(pCtx);

    // -- INTERFACE 0: CONTROL DATA --
    // set up the classes
    libusbd_iface_set_class(pCtx, X360_IF_CONTROL, 0xFF);
    libusbd_iface_set_subclass(pCtx, X360_IF_CONTROL, 0x5D);
    libusbd_iface_set_protocol(pCtx, X360_IF_CONTROL, 0x1);
    // set the extra descriptor. contains xinput data e.g. flags, type and subtype
    char xinput_type = 0x01;
    char xinput_subtype = 0x01;
    uint8_t control_report_desc[] = { 0x11, 0x21, 0x00, xinput_type, xinput_subtype, 0x25, 0x81, 0x14, 0x00, 0x00, 0x00, 0x00, 0x13, 0x01, 0x08, 0x00, 0x00 };
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
    // set up the classes
    libusbd_iface_set_class(pCtx, X360_IF_SECURITY, 0xFF);
    libusbd_iface_set_subclass(pCtx, X360_IF_SECURITY, 0xFD);
    libusbd_iface_set_protocol(pCtx, X360_IF_SECURITY, 0x13);
    // set the interface description
    libusbd_iface_set_description(pCtx, X360_IF_SECURITY, "Xbox Security Method 3, Version 1.00, \xA9 2099 Microsoft Corporation. All rights reserved.");
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
        //controls.left_trigger = rand() & 0xFF;
        //controls.right_trigger = rand() & 0xFF;
        controls.buttons1 = rand() & 0x0F;
        //controls.buttons2 = rand() & 0xF3; // don't press guide. steam fucking sucks.
        // boogie woogie
        rotate_cycle += 1000;
    }

    libusbd_free(pCtx);

    printf("asd\n");
}