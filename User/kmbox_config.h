/******************************************************************************
 * File Name          : kmbox_config.h
 * Description        : CH32V307VCT6 KMBox Net Configuration & Protocol Header
 *                      - USBFS (PA11/PA12): USB Host (Physical Mouse Input)
 *                      - ETH   (10M PHY)  : UDP Server (PC Network Input)
 *                      - USBHS (PB6/PB7)  : 1000Hz USB HID Mouse Device (Output)
 ******************************************************************************/
#ifndef __KMBOX_CONFIG_H
#define __KMBOX_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/******************************************************************************/
/* 1. Network Configuration (WCHNET 10M Internal PHY, Static IP)              */
/******************************************************************************/
#define KMBOX_IP_ADDR0              192
#define KMBOX_IP_ADDR1              168
#define KMBOX_IP_ADDR2              2
#define KMBOX_IP_ADDR3              188

#define KMBOX_GW_ADDR0              192
#define KMBOX_GW_ADDR1              168
#define KMBOX_GW_ADDR2              2
#define KMBOX_GW_ADDR3              1

#define KMBOX_MASK0                 255
#define KMBOX_MASK1                 255
#define KMBOX_MASK2                 255
#define KMBOX_MASK3                 0

#define KMBOX_UDP_PORT              8888

/******************************************************************************/
/* 2. USB HID Device Identity & Dynamic VID/PID Cloning                       */
/******************************************************************************/
/* Default fallback identity: Logitech USB Optical Mouse (046D:C077) */
#define KMBOX_DEFAULT_VID           0x046D
#define KMBOX_DEFAULT_PID           0xC077

/* Set to 1 to automatically clone the physical mouse's VID/PID onto USBHS    */
#define KMBOX_CLONE_HOST_VID_PID    1

/******************************************************************************/
/* 3. UDP Binary Packet Protocol (9 Bytes, Little-Endian)                     */
/******************************************************************************/
#define KMBOX_PKT_HEAD1             0x55
#define KMBOX_PKT_HEAD2             0xAA

typedef struct __attribute__((packed))
{
    uint8_t  head1;     /* 0x55                                               */
    uint8_t  head2;     /* 0xAA                                               */
    uint8_t  buttons;   /* Bit0:L, Bit1:R, Bit2:M, Bit3:Back, Bit4:Forward    */
    int16_t  dx;        /* Signed 16-bit X delta                              */
    int16_t  dy;        /* Signed 16-bit Y delta                              */
    int8_t   wheel;     /* Signed 8-bit vertical wheel delta                  */
    uint8_t  checksum;  /* (buttons + dx_lo + dx_hi + dy_lo + dy_hi + wheel)  */
} KmboxUdpPacket;

/******************************************************************************/
/* 4. Mouse Vector Compositor API                                             */
/******************************************************************************/
void kmbox_add_host_mouse( uint8_t buttons, int16_t dx, int16_t dy, int8_t wheel );
void kmbox_host_mouse_disconnected( void );
void kmbox_add_udp_mouse( uint8_t buttons, int16_t dx, int16_t dy, int8_t wheel );
void kmbox_parse_udp_payload( const uint8_t *buf, uint32_t len );
void kmbox_compositor_1ms_tick( void );
void kmbox_compositor_task( void );
void kmbox_clone_device_vid_pid( uint16_t vid, uint16_t pid );

#ifdef __cplusplus
}
#endif

#endif /* __KMBOX_CONFIG_H */
