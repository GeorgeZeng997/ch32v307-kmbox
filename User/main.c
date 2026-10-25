/******************************************************************************
 * File Name          : main.c
 * Description        : CH32V307VCT6 KMBox Net Main Firmware
 *                      - USBFS (PA11/PA12): USB Host (USB 2.0 Hub + Physical KM)
 *                      - ETH   (10M PHY)  : UDP Server (192.168.2.188:8888)
 *                      - USBHS (PB6/PB7)  : 1000Hz Composite HID KM to PC
 ******************************************************************************/
#include <string.h>
#include <stdlib.h>
#include "debug.h"
#include "eth_driver.h"
#include "usb_host_config.h"
#include "ch32v30x_usbhs_device.h"
#include "usbd_desc.h"
#include "kmbox_config.h"

/******************************************************************************/
/* Network Globals                                                            */
/******************************************************************************/
#define UDP_RECE_BUF_LEN            1472

static u8  MACAddr[ 6 ];
static u8  IPAddr[ 4 ]   = { KMBOX_IP_ADDR0, KMBOX_IP_ADDR1, KMBOX_IP_ADDR2, KMBOX_IP_ADDR3 };
static u8  GWIPAddr[ 4 ] = { KMBOX_GW_ADDR0, KMBOX_GW_ADDR1, KMBOX_GW_ADDR2, KMBOX_GW_ADDR3 };
static u8  IPMask[ 4 ]   = { KMBOX_MASK0,    KMBOX_MASK1,    KMBOX_MASK2,    KMBOX_MASK3    };
static u16 SrcPort       = KMBOX_UDP_PORT;

static u8  SocketId;
static u8  SocketRecvBuf[ UDP_RECE_BUF_LEN ];

/******************************************************************************/
/* 1000Hz Mouse Vector Compositor State (Endpoint 2 IN)                       */
/******************************************************************************/
static volatile uint8_t g_host_buttons      = 0;
static volatile uint8_t g_udp_buttons       = 0;
static volatile uint8_t g_last_sent_buttons = 0;
static volatile int32_t g_acc_dx            = 0;
static volatile int32_t g_acc_dy            = 0;
static volatile int16_t g_acc_wheel         = 0;
static volatile uint8_t g_mouse_dirty       = 0;
static volatile uint8_t g_tick_1ms          = 0;

/******************************************************************************/
/* Hardware Keyboard Merger & Masking State (Endpoint 1 IN)                   */
/******************************************************************************/
static uint8_t          g_host_kb[ 8 ]      = { 0 };
static uint8_t          g_udp_kb_mod        = 0;
static uint8_t          g_udp_keys[ 6 ]     = { 0 };
static uint8_t          g_masked_keys[ 6 ]  = { 0 };
static uint8_t          g_last_sent_kb[ 8 ] = { 0 };
static volatile uint8_t g_kb_dirty          = 0;

static inline int8_t clamp_int8( int32_t v )
{
    if( v > 127 )  return 127;
    if( v < -127 ) return -127;
    return (int8_t)v;
}

static bool key_in_array( uint8_t key, const uint8_t *arr, uint8_t len )
{
    if( key == 0 ) return false;
    for( uint8_t i = 0; i < len; i++ )
    {
        if( arr[ i ] == key ) return true;
    }
    return false;
}

static void add_key_to_array( uint8_t key, uint8_t *arr, uint8_t len )
{
    if( key == 0 || key_in_array( key, arr, len ) ) return;
    for( uint8_t i = 0; i < len; i++ )
    {
        if( arr[ i ] == 0 )
        {
            arr[ i ] = key;
            return;
        }
    }
}

static void remove_key_from_array( uint8_t key, uint8_t *arr, uint8_t len )
{
    if( key == 0 ) return;
    for( uint8_t i = 0; i < len; i++ )
    {
        if( arr[ i ] == key )
        {
            arr[ i ] = 0;
        }
    }
}

/*********************************************************************
 * @fn      kmbox_add_host_mouse
 *
 * @brief   Accumulate physical USB mouse report from USBFS Host.
 */
void kmbox_add_host_mouse( uint8_t buttons, int16_t dx, int16_t dy, int8_t wheel )
{
    __disable_irq( );
    g_host_buttons = buttons & 0x1F;
    g_acc_dx      += dx;
    g_acc_dy      += dy;
    g_acc_wheel   += wheel;
    g_mouse_dirty  = 1;
    __enable_irq( );
}

/*********************************************************************
 * @fn      kmbox_host_mouse_disconnected
 *
 * @brief   Release physical mouse buttons when unplugged from USBFS.
 */
void kmbox_host_mouse_disconnected( void )
{
    __disable_irq( );
    g_host_buttons = 0;
    g_mouse_dirty  = 1;
    __enable_irq( );
}

/*********************************************************************
 * @fn      kmbox_add_udp_mouse
 *
 * @brief   Accumulate network UDP mouse command from PC.
 */
void kmbox_add_udp_mouse( uint8_t buttons, int16_t dx, int16_t dy, int8_t wheel )
{
    __disable_irq( );
    g_udp_buttons = buttons & 0x1F;
    g_acc_dx     += dx;
    g_acc_dy     += dy;
    g_acc_wheel  += wheel;
    g_mouse_dirty = 1;
    __enable_irq( );
}

/*********************************************************************
 * @fn      kmbox_update_host_keyboard
 *
 * @brief   Update physical keyboard state read from USBFS Host.
 */
void kmbox_update_host_keyboard( const uint8_t *pbuf, uint16_t len )
{
    if( pbuf == NULL || len < 8 )
    {
        return;
    }

    /* Handle optional 1-byte Report ID prefix on 9-byte keyboard reports */
    const uint8_t *rep = ( len == 9 && pbuf[ 2 ] == 0x00 ) ? ( pbuf + 1 ) : pbuf;

    __disable_irq( );
    memcpy( g_host_kb, rep, 8 );
    g_kb_dirty = 1;
    __enable_irq( );
}

/*********************************************************************
 * @fn      kmbox_host_keyboard_disconnected
 *
 * @brief   Clear physical keyboard state when unplugged from USBFS.
 */
void kmbox_host_keyboard_disconnected( void )
{
    __disable_irq( );
    memset( g_host_kb, 0, sizeof( g_host_kb ) );
    g_kb_dirty = 1;
    __enable_irq( );
}

/*********************************************************************
 * @fn      kmbox_update_udp_key
 *
 * @brief   Press, release, or mask a keyboard key via UDP command.
 */
void kmbox_update_udp_key( uint8_t action, uint8_t hid_keycode, uint8_t modifier )
{
    __disable_irq( );
    if( action == KMBOX_KEY_PRESS )
    {
        g_udp_kb_mod |= modifier;
        add_key_to_array( hid_keycode, g_udp_keys, 6 );
    }
    else if( action == KMBOX_KEY_RELEASE )
    {
        g_udp_kb_mod &= ~modifier;
        remove_key_from_array( hid_keycode, g_udp_keys, 6 );
    }
    else if( action == KMBOX_KEY_MASK_PHYS )
    {
        add_key_to_array( hid_keycode, g_masked_keys, 6 );
    }
    else if( action == KMBOX_KEY_UNMASK_PHYS )
    {
        remove_key_from_array( hid_keycode, g_masked_keys, 6 );
    }
    g_kb_dirty = 1;
    __enable_irq( );
}

/*********************************************************************
 * @fn      kmbox_compositor_1ms_tick
 *
 * @brief   Called from TIM3_IRQHandler at 1000Hz (every 1ms).
 */
void kmbox_compositor_1ms_tick( void )
{
    g_tick_1ms = 1;
}

/*********************************************************************
 * @fn      kmbox_compositor_task
 *
 * @brief   1000Hz USBHS Composite HID Keyboard (EP1) + Mouse (EP2) task.
 */
void kmbox_compositor_task( void )
{
    if( USBHS_DevEnumStatus == 0 )
    {
        return;
    }

    /* 1. Hardware Keyboard Merger (Endpoint 1 IN: immediate upload on change) */
    if( g_kb_dirty && ( ( USBHS_Endp_Busy[ DEF_UEP1 ] & DEF_UEP_BUSY ) == 0 ) )
    {
        uint8_t merged_kb[ 8 ] = { 0 };
        uint8_t out_idx = 2;

        __disable_irq( );
        g_kb_dirty = 0;
        merged_kb[ 0 ] = g_host_kb[ 0 ] | g_udp_kb_mod;
        merged_kb[ 1 ] = 0x00;

        /* Copy physical keyboard keys unless masked by hardware counter-strafe */
        for( uint8_t i = 2; i < 8 && out_idx < 8; i++ )
        {
            uint8_t k = g_host_kb[ i ];
            if( k != 0 && !key_in_array( k, g_masked_keys, 6 ) && !key_in_array( k, &merged_kb[ 2 ], 6 ) )
            {
                merged_kb[ out_idx++ ] = k;
            }
        }

        /* Merge UDP-injected keys onto the same USB keyboard interface */
        for( uint8_t i = 0; i < 6 && out_idx < 8; i++ )
        {
            uint8_t k = g_udp_keys[ i ];
            if( k != 0 && !key_in_array( k, &merged_kb[ 2 ], 6 ) )
            {
                merged_kb[ out_idx++ ] = k;
            }
        }
        __enable_irq( );

        if( memcmp( merged_kb, g_last_sent_kb, 8 ) != 0 )
        {
            memcpy( g_last_sent_kb, merged_kb, 8 );
            USBHS_Endp_DataUp( DEF_UEP1, merged_kb, 8, DEF_UEP_CPY_LOAD );
        }
    }

    /* 2. 1000Hz Hardware Mouse Vector Compositor (Endpoint 2 IN) */
    if( !g_tick_1ms || ( USBHS_Endp_Busy[ DEF_UEP2 ] & DEF_UEP_BUSY ) )
    {
        return;
    }

    g_tick_1ms = 0;

    uint8_t merged_btn;
    int8_t  send_dx;
    int8_t  send_dy;
    int8_t  send_wheel;

    __disable_irq( );
    merged_btn = ( g_host_buttons | g_udp_buttons ) & 0x1F;

    if( !g_mouse_dirty && merged_btn == g_last_sent_buttons &&
        g_acc_dx == 0 && g_acc_dy == 0 && g_acc_wheel == 0 )
    {
        __enable_irq( );
        return;
    }

    send_dx    = clamp_int8( g_acc_dx );
    send_dy    = clamp_int8( g_acc_dy );
    send_wheel = clamp_int8( g_acc_wheel );

    g_acc_dx    -= send_dx;
    g_acc_dy    -= send_dy;
    g_acc_wheel -= send_wheel;

    if( g_acc_dx == 0 && g_acc_dy == 0 && g_acc_wheel == 0 )
    {
        g_mouse_dirty = 0;
    }
    g_last_sent_buttons = merged_btn;
    __enable_irq( );

    uint8_t report[ 4 ];
    report[ 0 ] = merged_btn;
    report[ 1 ] = (uint8_t)send_dx;
    report[ 2 ] = (uint8_t)send_dy;
    report[ 3 ] = (uint8_t)send_wheel;

    USBHS_Endp_DataUp( DEF_UEP2, report, 4, DEF_UEP_CPY_LOAD );
}

/*********************************************************************
 * @fn      kmbox_clone_device_vid_pid
 *
 * @brief   Clone physical mouse VID/PID onto USBHS Device descriptor.
 */
void kmbox_clone_device_vid_pid( uint16_t vid, uint16_t pid )
{
    uint16_t cur_vid = (uint16_t)MyDevDescr[ 8 ] | ( (uint16_t)MyDevDescr[ 9 ] << 8 );
    uint16_t cur_pid = (uint16_t)MyDevDescr[ 10 ] | ( (uint16_t)MyDevDescr[ 11 ] << 8 );

    if( cur_vid == vid && cur_pid == pid )
    {
        return;
    }

    printf( "Cloning Host Mouse VID:PID -> %04X:%04X\r\n", vid, pid );
    MyDevDescr[ 8 ]  = (uint8_t)( vid & 0xFF );
    MyDevDescr[ 9 ]  = (uint8_t)( ( vid >> 8 ) & 0xFF );
    MyDevDescr[ 10 ] = (uint8_t)( pid & 0xFF );
    MyDevDescr[ 11 ] = (uint8_t)( ( pid >> 8 ) & 0xFF );

    /* Trigger soft re-attachment on USBHS so PC enumerates new VID:PID */
    USBHS_Device_Init( DISABLE );
    Delay_Ms( 50 );
    USBHS_Device_Init( ENABLE );
}

/*********************************************************************
 * @fn      kmbox_parse_udp_payload
 *
 * @brief   Parse incoming UDP datagram:
 *          - 0x55 0xAA: 9-byte Binary Mouse Packet
 *          - 0x55 0xBB: 9-byte Binary Keyboard Packet
 *          - "55 btn x y wheel pan sum": Legacy ASCII Mouse Packet
 */
void kmbox_parse_udp_payload( const uint8_t *buf, uint32_t len )
{
    if( buf == NULL || len == 0 )
    {
        return;
    }

    /* 1. Binary Fast-Path: 9-byte KmboxUdpPacket (0x55 0xAA or 0x55 0xBB) */
    if( len >= sizeof( KmboxUdpPacket ) &&
        buf[ 0 ] == KMBOX_PKT_HEAD1 &&
        ( buf[ 1 ] == KMBOX_PKT_HEAD2_MOUSE || buf[ 1 ] == KMBOX_PKT_HEAD2_KEY ) )
    {
        uint32_t offset = 0;
        while( offset + sizeof( KmboxUdpPacket ) <= len )
        {
            const uint8_t *p = buf + offset;
            if( p[ 0 ] != KMBOX_PKT_HEAD1 )
            {
                break;
            }
            uint8_t calc_sum = (uint8_t)( p[ 2 ] + p[ 3 ] + p[ 4 ] + p[ 5 ] + p[ 6 ] + p[ 7 ] );
            if( calc_sum == p[ 8 ] )
            {
                if( p[ 1 ] == KMBOX_PKT_HEAD2_MOUSE )
                {
                    uint8_t buttons = p[ 2 ] & 0x1F;
                    int16_t dx      = (int16_t)( (uint16_t)p[ 3 ] | ( (uint16_t)p[ 4 ] << 8 ) );
                    int16_t dy      = (int16_t)( (uint16_t)p[ 5 ] | ( (uint16_t)p[ 6 ] << 8 ) );
                    int8_t  wheel   = (int8_t)p[ 7 ];
                    kmbox_add_udp_mouse( buttons, dx, dy, wheel );
                }
                else if( p[ 1 ] == KMBOX_PKT_HEAD2_KEY )
                {
                    uint8_t action      = p[ 2 ];
                    uint8_t hid_keycode = p[ 3 ];
                    uint8_t modifier    = p[ 7 ];
                    kmbox_update_udp_key( action, hid_keycode, modifier );
                }
            }
            offset += sizeof( KmboxUdpPacket );
        }
        return;
    }

    /* 2. Backward-Compatible ASCII Text Protocol: "55 btn x y wheel pan sum" */
    if( len >= 12 && len < 64 && buf[ 0 ] == '5' && buf[ 1 ] == '5' && buf[ 2 ] == ' ' )
    {
        char line[ 64 ];
        memcpy( line, buf, len );
        line[ len ] = '\0';

        char *ptr = line;
        long vals[ 7 ];
        for( int i = 0; i < 7; i++ )
        {
            char *endptr;
            vals[ i ] = strtol( ptr, &endptr, 10 );
            if( ptr == endptr )
            {
                return;
            }
            ptr = endptr;
        }

        long calc_sum = ( vals[ 1 ] + vals[ 2 ] + vals[ 3 ] + vals[ 4 ] + vals[ 5 ] ) % 256;
        if( vals[ 0 ] == 55 && ( calc_sum == vals[ 6 ] || ( ( calc_sum & 0xFF ) == ( vals[ 6 ] & 0xFF ) ) ) )
        {
            kmbox_add_udp_mouse( (uint8_t)vals[ 1 ],
                                 (int16_t)vals[ 2 ],
                                 (int16_t)vals[ 3 ],
                                 (int8_t)vals[ 4 ] );
        }
    }
}

/*********************************************************************
 * @fn      TIM2_Init
 *
 * @brief   Initializes TIM2 for WCHNET Ethernet stack timing.
 */
static void TIM2_Init( void )
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = { 0 };

    RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM2, ENABLE );

    TIM_TimeBaseStructure.TIM_Period        = SystemCoreClock / 1000000;
    TIM_TimeBaseStructure.TIM_Prescaler     = WCHNETTIMERPERIOD * 1000 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit( TIM2, &TIM_TimeBaseStructure );
    TIM_ITConfig( TIM2, TIM_IT_Update, ENABLE );

    TIM_Cmd( TIM2, ENABLE );
    TIM_ClearITPendingBit( TIM2, TIM_IT_Update );
    NVIC_EnableIRQ( TIM2_IRQn );
}

/*********************************************************************
 * @fn      WCHNET_UdpServerRecv
 *
 * @brief   WCHNET UDP socket receive callback (zero-copy buffer).
 */
void WCHNET_UdpServerRecv( struct _SOCK_INF *socinf, u32 ipaddr, u16 port, u8 *buf, u32 len )
{
    (void)socinf;
    (void)ipaddr;
    (void)port;
    kmbox_parse_udp_payload( buf, len );
}

/*********************************************************************
 * @fn      WCHNET_CreateUdpSocket
 *
 * @brief   Create UDP server socket listening on KMBOX_UDP_PORT (8888).
 */
static void WCHNET_CreateUdpSocket( void )
{
    u8 i;
    SOCK_INF TmpSocketInf;

    memset( (void *)&TmpSocketInf, 0, sizeof( SOCK_INF ) );
    TmpSocketInf.SourPort       = SrcPort;
    TmpSocketInf.ProtoType      = PROTO_TYPE_UDP;
    TmpSocketInf.RecvBufLen     = UDP_RECE_BUF_LEN;
    TmpSocketInf.RecvStartPoint = (u32)SocketRecvBuf;
    TmpSocketInf.AppCallBack    = WCHNET_UdpServerRecv;
    i = WCHNET_SocketCreat( &SocketId, &TmpSocketInf );
    if( i == WCHNET_ERR_SUCCESS )
    {
        printf( "UDP Server Listening on %d.%d.%d.%d:%d (SocketId=%d)\r\n",
                IPAddr[ 0 ], IPAddr[ 1 ], IPAddr[ 2 ], IPAddr[ 3 ],
                SrcPort, SocketId );
    }
    else
    {
        printf( "WCHNET_SocketCreat Error: %02X\r\n", i );
    }
}

/*********************************************************************
 * @fn      WCHNET_HandleGlobalInt
 *
 * @brief   Process WCHNET global and socket interrupts.
 */
static void WCHNET_HandleGlobalInt( void )
{
    u8  intstat;
    u16 i;

    intstat = WCHNET_GetGlobalInt( );
    if( intstat & GINT_STAT_PHY_CHANGE )
    {
        i = WCHNET_GetPHYStatus( );
        if( i & PHY_Linked_Status )
        {
            printf( "ETH PHY Link Up (10Mbps Full-Duplex)\r\n" );
        }
    }
    if( intstat & GINT_STAT_SOCKET )
    {
        for( i = 0; i < WCHNET_MAX_SOCKET_NUM; i++ )
        {
            WCHNET_GetSocketInt( i );
        }
    }
}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main entry point for CH32V307VCT6 KMBox Net firmware.
 */
int main( void )
{
    u8 eth_ret;

    NVIC_PriorityGroupConfig( NVIC_PriorityGroup_2 );
    SystemCoreClockUpdate( );
    Delay_Init( );
    USART_Printf_Init( 115200 );

    printf( "\r\n========================================\r\n" );
    printf( " CH32V307VCT6 KMBox Net Firmware v1.1\r\n" );
    printf( " SystemClk : %lu Hz\r\n", (unsigned long)SystemCoreClock );
    printf( " ChipID    : %08lx\r\n", (unsigned long)DBGMCU_GetCHIPID( ) );
    printf( "========================================\r\n" );

    /* 1. Initialize TIM3 (1ms / 1000Hz tick for USBFS Host + USBHS Compositor) */
    TIM3_Init( 9, SystemCoreClock / 10000 - 1 );

    /* 2. Initialize USBHS Device (Port 2 PB6/PB7 -> PC 1000Hz Composite KM)
     *    Note: USBHS_RCC_Init() enables the 48MHz USBPHY PLL shared with USBFS */
    USBHS_RCC_Init( );
    USBHS_Device_Init( ENABLE );
    printf( "USBHS Device Init OK (1000Hz Composite Keyboard + Mouse)\r\n" );

    /* 3. Initialize USBFS Host (Port 1 PA11/PA12 -> USB Hub + Physical KM) */
#if DEF_USBFS_PORT_EN
    USBFS_RCC_Init( );
    USBFS_Host_Init( ENABLE );
    memset( &RootHubDev[ DEF_USBFS_PORT_INDEX ].bStatus, 0, sizeof( ROOT_HUB_DEVICE ) );
    memset( &HostCtl[ DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL ].InterfaceNum,
            0, DEF_ONE_USB_SUP_DEV_TOTAL * sizeof( HOST_CTL ) );
    printf( "USBFS Host Init OK (Hub + Physical KM Input)\r\n" );
#endif

    /* 4. Initialize Internal 10M Ethernet PHY + WCHNET UDP Server */
    WCHNET_GetMacAddr( MACAddr );
    printf( "ETH MAC   : %02X:%02X:%02X:%02X:%02X:%02X\r\n",
            MACAddr[ 0 ], MACAddr[ 1 ], MACAddr[ 2 ],
            MACAddr[ 3 ], MACAddr[ 4 ], MACAddr[ 5 ] );

    TIM2_Init( );
    eth_ret = ETH_LibInit( IPAddr, GWIPAddr, IPMask, MACAddr );
    if( eth_ret == WCHNET_ERR_SUCCESS )
    {
        WCHNET_CreateUdpSocket( );
    }
    else
    {
        printf( "ETH_LibInit Warning: 0x%02X (USB Passthrough still active)\r\n", eth_ret );
    }

    /* 5. Main Real-Time Loop */
    while( 1 )
    {
        if( eth_ret == WCHNET_ERR_SUCCESS )
        {
            WCHNET_MainTask( );
            if( WCHNET_QueryGlobalInt( ) )
            {
                WCHNET_HandleGlobalInt( );
            }
        }

        USBH_MainDeal( );
        kmbox_compositor_task( );
    }
}
