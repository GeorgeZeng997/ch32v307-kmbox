/********************************** (C) COPYRIGHT *******************************
 * File Name          : usbd_desc.h
 * Author             : WCH / KMBox Net
 * Version            : V1.1.0
 * Description        : USB descriptors for 1000Hz Composite HID Keyboard + 5-Button Mouse.
 *******************************************************************************/
#ifndef __USBD_DESC_H
#define __USBD_DESC_H

#include "stdint.h"
#include "kmbox_config.h"

/* File Version */
#define DEF_FILE_VERSION              0x01

/* Default USB Device Info (Logitech, dynamically clonable from physical mouse) */
#define DEF_USB_VID                   KMBOX_DEFAULT_VID
#define DEF_USB_PID                   KMBOX_DEFAULT_PID

/* USB Device Descriptor, Device Serial Number(bcdDevice) */
#define DEF_IC_PRG_VER                DEF_FILE_VERSION

/* USB Device Endpoint Size */
#define DEF_USBD_UEP0_SIZE            64     /* usb hs/fs device end-point 0 size */
/* HS */
#define DEF_USBD_HS_PACK_SIZE         64     /* hid int pack size */
/* FS */
#define DEF_USBD_FS_PACK_SIZE         64     /* usb fs device max bulk/int pack size */

/* Endpoint 1 (Keyboard IN) & Endpoint 2 (Mouse IN) Size */
#define DEF_USB_EP1_HS_SIZE           DEF_USBD_HS_PACK_SIZE
#define DEF_USB_EP2_HS_SIZE           DEF_USBD_HS_PACK_SIZE
#define DEF_USB_EP1_FS_SIZE           DEF_USBD_FS_PACK_SIZE
#define DEF_USB_EP2_FS_SIZE           DEF_USBD_FS_PACK_SIZE

/* USB Device Descriptor Length */
#define DEF_USBD_DEVICE_DESC_LEN      ( (uint16_t)MyDevDescr[ 0 ] )
#define DEF_USBD_CONFIG_DESC_LEN      ( (uint16_t)MyCfgDescr[ 2 ] + ( (uint16_t)MyCfgDescr[ 3 ] << 8 ) )
#define DEF_USBD_REPORT_DESC_LEN_KB   0x3E
#define DEF_USBD_REPORT_DESC_LEN_MS   0x34
#define DEF_USBD_LANG_DESC_LEN        ( (uint16_t)MyLangDescr[ 0 ] )
#define DEF_USBD_MANU_DESC_LEN        ( (uint16_t)MyManuInfo[ 0 ] )
#define DEF_USBD_PROD_DESC_LEN        ( (uint16_t)MyProdInfo[ 0 ] )
#define DEF_USBD_SN_DESC_LEN          ( (uint16_t)MySerNumInfo[ 0 ] )
#define DEF_USBD_QUALFY_DESC_LEN      ( (uint16_t)MyQuaDesc[ 0 ] )
#define DEF_USBD_BOS_DESC_LEN         0
#define DEF_USBD_FS_OTH_DESC_LEN      0
#define DEF_USBD_HS_OTH_DESC_LEN      0

/* Descriptor Declaration (MyDevDescr is mutable for dynamic VID/PID cloning) */
extern uint8_t       MyDevDescr[ ];
extern const uint8_t MyCfgDescr[ ];
extern const uint8_t KeyRepDesc[ ];
extern const uint8_t MouseRepDesc[ ];
extern const uint8_t MyQuaDesc[ ];
extern const uint8_t MyLangDescr[ ];
extern const uint8_t MyManuInfo[ ];
extern const uint8_t MyProdInfo[ ];
extern const uint8_t MySerNumInfo[ ];

#endif /* __USBD_DESC_H */
