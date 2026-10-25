/******************************************************************************
 * File Name          : main.c
 * Description        : CH32V307VCT6 KMBox Net Main Firmware
 *                      - USBFS (PA11/PA12): USB Host reading physical USB Mouse
 *                      - ETH   (10M PHY)  : UDP Server (192.168.2.188:8888)
 *                      - USBHS (PB6/PB7)  : 1000Hz USB HID Mouse Device to PC
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
/* 1000Hz Mouse Vector Compositor State                                       */
/******************************************************************************/
static volatile uint8_t g_host_buttons      = 0;
static volatile uint8_t g_udp_buttons       = 0;
static volatile uint8_t g_last_sent_buttons = 0;
static volatile int32_t g_acc_dx            = 0;
static volatile int32_t g_acc_dy            = 0;
static volatile int16_t g_acc_wheel         = 0;
static volatile uint8_t g_dirty             = 0;
static volatile uint8_t g_tick_1ms          = 0;

static inline int8_t clamp_int8( int32_t v )
{
    if( v > 127 )  return 127;
    if( v < -127 ) return -127;
    return (int8_t)v;
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
    g_dirty        = 1;
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
    g_dirty        = 1;
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
    g_dirty       = 1;
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
 * @brief   1000Hz USBHS HID Mouse report generator with remainder carry-over.
 */
void kmbox_compositor_task( void )
{
    if( !g_tick_1ms )
    {
        return;
    }

    /* Wait until USBHS is enumerated by the PC and Endpoint 1 IN is ready */
    if( USBHS_DevEnumStatus == 0 || ( USBHS_Endp_Busy[ DEF_UEP1 ] & DEF_UEP_BUSY ) )
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

    if( !g_dirty && merged_btn == g_last_sent_buttons &&
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
        g_dirty = 0;
    }
    g_last_sent_buttons = merged_btn;
    __enable_irq( );

    uint8_t report[ 4 ];
    report[ 0 ] = merged_btn;
    report[ 1 ] = (uint8_t)send_dx;
    report[ 2 ] = (uint8_t)send_dy;
    report[ 3 ] = (uint8_t)send_wheel;

    USBHS_Endp_DataUp( DEF_UEP1, report, 4, DEF_UEP_CPY_LOAD );
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
 * @brief   Parse incoming UDP datagram (Binary 9-byte KmboxUdpPacket
 *          or ASCII "55 btn x y wheel pan sum" format).
 */
void kmbox_parse_udp_payload( const uint8_t *buf, uint32_t len )
{
    if( buf == NULL || len == 0 )
    {
        return;
    }

    /* 1. Binary Fast-Path: 9-byte KmboxUdpPacket (0x55 0xAA) */
    if( len >= sizeof( KmboxUdpPacket ) &&
        buf[ 0 ] == KMBOX_PKT_HEAD1 &&
        buf[ 1 ] == KMBOX_PKT_HEAD2 )
    {
        uint32_t offset = 0;
        while( offset + sizeof( KmboxUdpPacket ) <= len )
        {
            const uint8_t *p = buf + offset;
            if( p[ 0 ] != KMBOX_PKT_HEAD1 || p[ 1 ] != KMBOX_PKT_HEAD2 )
            {
                break;
            }
            uint8_t calc_sum = (uint8_t)( p[ 2 ] + p[ 3 ] + p[ 4 ] + p[ 5 ] + p[ 6 ] + p[ 7 ] );
            if( calc_sum == p[ 8 ] )
            {
                uint8_t buttons = p[ 2 ] & 0x1F;
                int16_t dx      = (int16_t)( (uint16_t)p[ 3 ] | ( (uint16_t)p[ 4 ] << 8 ) );
                int16_t dy      = (int16_t)( (uint16_t)p[ 5 ] | ( (uint16_t)p[ 6 ] << 8 ) );
                int8_t  wheel   = (int8_t)p[ 7 ];
                kmbox_add_udp_mouse( buttons, dx, dy, wheel );
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
    printf( " CH32V307VCT6 KMBox Net Firmware v1.0\r\n" );
    printf( " SystemClk : %lu Hz\r\n", (unsigned long)SystemCoreClock );
    printf( " ChipID    : %08lx\r\n", (unsigned long)DBGMCU_GetCHIPID( ) );
    printf( "========================================\r\n" );

    /* 1. Initialize TIM3 (1ms / 1000Hz tick for USBFS Host + USBHS Compositor) */
    TIM3_Init( 9, SystemCoreClock / 10000 - 1 );

    /* 2. Initialize USBHS Device (Port 2 PB6/PB7 -> PC 1000Hz HID Mouse)
     *    Note: USBHS_RCC_Init() enables the 48MHz USBPHY PLL shared with USBFS */
    USBHS_RCC_Init( );
    USBHS_Device_Init( ENABLE );
    printf( "USBHS Device Init OK (1000Hz HID Mouse)\r\n" );

    /* 3. Initialize USBFS Host (Port 1 PA11/PA12 -> Physical USB Mouse) */
#if DEF_USBFS_PORT_EN
    USBFS_RCC_Init( );
    USBFS_Host_Init( ENABLE );
    memset( &RootHubDev[ DEF_USBFS_PORT_INDEX ].bStatus, 0, sizeof( ROOT_HUB_DEVICE ) );
    memset( &HostCtl[ DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL ].InterfaceNum,
            0, DEF_ONE_USB_SUP_DEV_TOTAL * sizeof( HOST_CTL ) );
    printf( "USBFS Host Init OK (Physical Mouse Input)\r\n" );
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
