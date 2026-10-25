/********************************** (C) COPYRIGHT *******************************
 * File Name          : usbd_desc.c
 * Author             : WCH / KMBox Net
 * Version            : V1.1.0
 * Description        : 1000Hz Composite HID Keyboard + 5-Button Mouse Descriptors.
 *******************************************************************************/
#include "usbd_desc.h"

/* Device Descriptor (Mutable in RAM so VID/PID can clone attached USB mouse) */
uint8_t MyDevDescr[ ] =
{
    0x12,                                                   // bLength (18 bytes)
    0x01,                                                   // bDescriptorType (Device)
    0x00, 0x02,                                             // bcdUSB (USB 2.0)
    0x00,                                                   // bDeviceClass (Defined at interface level)
    0x00,                                                   // bDeviceSubClass
    0x00,                                                   // bDeviceProtocol
    DEF_USBD_UEP0_SIZE,                                     // bMaxPacketSize0 (64 bytes)
    (uint8_t)( DEF_USB_VID & 0xFF ),                        // idVendor Lo
    (uint8_t)( ( DEF_USB_VID >> 8 ) & 0xFF ),               // idVendor Hi
    (uint8_t)( DEF_USB_PID & 0xFF ),                        // idProduct Lo
    (uint8_t)( ( DEF_USB_PID >> 8 ) & 0xFF ),               // idProduct Hi
    0x00, DEF_IC_PRG_VER,                                   // bcdDevice
    0x01,                                                   // iManufacturer
    0x02,                                                   // iProduct
    0x03,                                                   // iSerialNumber
    0x01                                                    // bNumConfigurations
};

/* Configuration Descriptor Set (Interface 0: Keyboard, Interface 1: 5-Button Mouse, 59 bytes = 0x3B) */
const uint8_t MyCfgDescr[ ] =
{
    /* Configuration Descriptor */
    0x09,                                                   // bLength
    0x02,                                                   // bDescriptorType (Configuration)
    0x3B, 0x00,                                             // wTotalLength (59 bytes)
    0x02,                                                   // bNumInterfaces (2: Keyboard + Mouse)
    0x01,                                                   // bConfigurationValue
    0x00,                                                   // iConfiguration
    0xA0,                                                   // bmAttributes: Bus Powered; Remote Wakeup
    0x32,                                                   // MaxPower: 100mA

    /* Interface Descriptor (Keyboard: Interface 0) */
    0x09,                                                   // bLength
    0x04,                                                   // bDescriptorType (Interface)
    0x00,                                                   // bInterfaceNumber (0)
    0x00,                                                   // bAlternateSetting
    0x01,                                                   // bNumEndpoints (1)
    0x03,                                                   // bInterfaceClass (HID)
    0x01,                                                   // bInterfaceSubClass (Boot Interface)
    0x01,                                                   // bInterfaceProtocol (Keyboard)
    0x00,                                                   // iInterface

    /* HID Descriptor (Keyboard, offset 18) */
    0x09,                                                   // bLength
    0x21,                                                   // bDescriptorType (HID)
    0x11, 0x01,                                             // bcdHID (1.11)
    0x00,                                                   // bCountryCode
    0x01,                                                   // bNumDescriptors
    0x22,                                                   // bDescriptorType (Report)
    DEF_USBD_REPORT_DESC_LEN_KB, 0x00,                      // wDescriptorLength (62 bytes = 0x3E)

    /* Endpoint Descriptor (Keyboard EP1 IN: 1000Hz Polling) */
    0x07,                                                   // bLength
    0x05,                                                   // bDescriptorType (Endpoint)
    0x81,                                                   // bEndpointAddress: IN Endpoint 1
    0x03,                                                   // bmAttributes: Interrupt
    0x08, 0x00,                                             // wMaxPacketSize: 8 bytes
    0x01,                                                   // bInterval: 1ms (1000Hz)

    /* Interface Descriptor (Mouse: Interface 1) */
    0x09,                                                   // bLength
    0x04,                                                   // bDescriptorType (Interface)
    0x01,                                                   // bInterfaceNumber (1)
    0x00,                                                   // bAlternateSetting
    0x01,                                                   // bNumEndpoints (1)
    0x03,                                                   // bInterfaceClass (HID)
    0x01,                                                   // bInterfaceSubClass (Boot Interface)
    0x02,                                                   // bInterfaceProtocol (Mouse)
    0x00,                                                   // iInterface

    /* HID Descriptor (Mouse, offset 43) */
    0x09,                                                   // bLength
    0x21,                                                   // bDescriptorType (HID)
    0x11, 0x01,                                             // bcdHID (1.11)
    0x00,                                                   // bCountryCode
    0x01,                                                   // bNumDescriptors
    0x22,                                                   // bDescriptorType (Report)
    DEF_USBD_REPORT_DESC_LEN_MS, 0x00,                      // wDescriptorLength (52 bytes = 0x34)

    /* Endpoint Descriptor (Mouse EP2 IN: 1000Hz Polling) */
    0x07,                                                   // bLength
    0x05,                                                   // bDescriptorType (Endpoint)
    0x82,                                                   // bEndpointAddress: IN Endpoint 2
    0x03,                                                   // bmAttributes: Interrupt
    0x08, 0x00,                                             // wMaxPacketSize: 8 bytes
    0x01                                                    // bInterval: 1ms (1000Hz in Full-Speed)
};

/* Standard 8-Byte Keyboard Report Descriptor (62 bytes = 0x3E) */
const uint8_t KeyRepDesc[ ] =
{
    0x05, 0x01,                                             // Usage Page (Generic Desktop)
    0x09, 0x06,                                             // Usage (Keyboard)
    0xA1, 0x01,                                             // Collection (Application)
    0x05, 0x07,                                             // Usage Page (Key Codes)
    0x19, 0xE0,                                             // Usage Minimum (224: Left Control)
    0x29, 0xE7,                                             // Usage Maximum (231: Right GUI)
    0x15, 0x00,                                             // Logical Minimum (0)
    0x25, 0x01,                                             // Logical Maximum (1)
    0x75, 0x01,                                             // Report Size (1)
    0x95, 0x08,                                             // Report Count (8)
    0x81, 0x02,                                             // Input (Data,Variable,Absolute)
    0x95, 0x01,                                             // Report Count (1)
    0x75, 0x08,                                             // Report Size (8)
    0x81, 0x01,                                             // Input (Constant) - Reserved byte
    0x95, 0x03,                                             // Report Count (3)
    0x75, 0x01,                                             // Report Size (1)
    0x05, 0x08,                                             // Usage Page (LEDs)
    0x19, 0x01,                                             // Usage Minimum (1)
    0x29, 0x03,                                             // Usage Maximum (3)
    0x91, 0x02,                                             // Output (Data,Variable,Absolute)
    0x95, 0x05,                                             // Report Count (5)
    0x75, 0x01,                                             // Report Size (1)
    0x91, 0x01,                                             // Output (Constant,Array,Absolute)
    0x95, 0x06,                                             // Report Count (6)
    0x75, 0x08,                                             // Report Size (8)
    0x26, 0xFF, 0x00,                                       // Logical Maximum (255)
    0x05, 0x07,                                             // Usage Page (Key Codes)
    0x19, 0x00,                                             // Usage Minimum (0)
    0x29, 0x91,                                             // Usage Maximum (145)
    0x81, 0x00,                                             // Input (Data,Array,Absolute)
    0xC0                                                    // End Collection
};

/* 5-Button + 8-bit X/Y/Wheel Mouse Report Descriptor (52 bytes = 0x34) */
const uint8_t MouseRepDesc[ ] =
{
    0x05, 0x01,                                             // Usage Page (Generic Desktop)
    0x09, 0x02,                                             // Usage (Mouse)
    0xA1, 0x01,                                             // Collection (Application)
    0x09, 0x01,                                             // Usage (Pointer)
    0xA1, 0x00,                                             // Collection (Physical)
    0x05, 0x09,                                             // Usage Page (Button)
    0x19, 0x01,                                             // Usage Minimum (Button 1: Left)
    0x29, 0x05,                                             // Usage Maximum (Button 5: Forward)
    0x15, 0x00,                                             // Logical Minimum (0)
    0x25, 0x01,                                             // Logical Maximum (1)
    0x75, 0x01,                                             // Report Size (1)
    0x95, 0x05,                                             // Report Count (5)
    0x81, 0x02,                                             // Input (Data,Variable,Absolute)
    0x75, 0x03,                                             // Report Size (3) - 3-bit padding
    0x95, 0x01,                                             // Report Count (1)
    0x81, 0x01,                                             // Input (Constant,Array,Absolute)
    0x05, 0x01,                                             // Usage Page (Generic Desktop)
    0x09, 0x30,                                             // Usage (X)
    0x09, 0x31,                                             // Usage (Y)
    0x09, 0x38,                                             // Usage (Wheel)
    0x15, 0x81,                                             // Logical Minimum (-127)
    0x25, 0x7F,                                             // Logical Maximum (127)
    0x75, 0x08,                                             // Report Size (8)
    0x95, 0x03,                                             // Report Count (3)
    0x81, 0x06,                                             // Input (Data,Variable,Relative)
    0xC0,                                                   // End Collection
    0xC0                                                    // End Collection
};

/* Qualifier Descriptor */
const uint8_t MyQuaDesc[ ] =
{
    0x0A,                                                   // bLength
    0x06,                                                   // bDescriptorType
    0x00, 0x02,                                             // bcdUSB
    0x00,                                                   // bDeviceClass
    0x00,                                                   // bDeviceSubClass
    0x00,                                                   // bDeviceProtocol
    0x40,                                                   // bMaxPacketSize0
    0x01,                                                   // bNumConfigurations
    0x00                                                    // bReserved
};

/* Language Descriptor (English US 0x0409) */
const uint8_t MyLangDescr[ ] =
{
    0x04,
    0x03,
    0x09,
    0x04
};

/* Manufacturer Descriptor: "Logitech" */
const uint8_t MyManuInfo[ ] =
{
    0x12,
    0x03,
    'L', 0, 'o', 0, 'g', 0, 'i', 0, 't', 0, 'e', 0, 'c', 0, 'h', 0
};

/* Product Information: "USB Optical Mouse" */
const uint8_t MyProdInfo[ ] =
{
    0x24,
    0x03,
    'U', 0, 'S', 0, 'B', 0, ' ', 0,
    'O', 0, 'p', 0, 't', 0, 'i', 0, 'c', 0, 'a', 0, 'l', 0, ' ', 0,
    'M', 0, 'o', 0, 'u', 0, 's', 0, 'e', 0
};

/* Serial Number Information */
const uint8_t MySerNumInfo[ ] =
{
    0x16,
    0x03,
    '0', 0, '1', 0, '2', 0, '3', 0, '4', 0,
    '5', 0, '6', 0, '7', 0, '8', 0, '9', 0
};
