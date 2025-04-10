/* Include Unity header */
#include <unity.h>

/* Include standard libraries */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "list.h"

#include "FreeRTOS_IP.h"
#include "FreeRTOS_IP_Private.h"

#define prvROUND_UP_TO( SIZE, ALIGNMENT )    ( ( ( SIZE ) + ( ALIGNMENT ) -1 ) / ( ALIGNMENT ) *( ALIGNMENT ) )

/* FIXME: Consider instead fixing ipBUFFER_PADDING if it's supposed to be pointer aligned. */
#define prvALIGNED_BUFFER_PADDING    prvROUND_UP_TO( ipBUFFER_PADDING, sizeof( void * ) )

struct xNetworkEndPoint * pxNetworkEndPoints = NULL;

NetworkInterface_t xInterfaces[ 1 ];

DHCPData_t xDHCPData;

volatile BaseType_t xInsideInterrupt = pdFALSE;

const MACAddress_t xDefault_MacAddress = { { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 } };

#define xSizeofUDPBuffer    300

Socket_t xDHCPSocket;
extern Socket_t xDHCPv4Socket;

eDHCPCallbackPhase_t eStubExpectedDHCPPhase;
struct xNetworkEndPoint * pxStubExpectedEndPoint;
uint32_t ulStubExpectedIPAddress;
eDHCPCallbackAnswer_t eStubExpectedReturn;
static uint8_t * ucGenericPtr;
static int32_t ulGenericLength;
static NetworkBufferDescriptor_t * pxGlobalNetworkBuffer[ 10 ];
static uint8_t GlobalBufferCounter = 0;

static uint8_t pucUDPBuffer[ xSizeofUDPBuffer ];

static uint8_t DHCP_header[] =
{
    dhcpREPLY_OPCODE,       /**< Operation Code: Specifies the general type of message. */
    0x01,                   /**< Hardware type used on the local network. */
    0x06,                   /**< Hardware Address Length: Specifies how long hardware
                             * addresses are in this message. */
    0x02,                   /**< Hops. */
    0x01, 0xAB, 0xCD, 0xEF, /**< A 32-bit identification field generated by the client,
                             * to allow it to match up the request with replies received
                             * from DHCP servers. */
    0x01,                   /**< Number of seconds elapsed since a client began an attempt to acquire or renew a lease. */
    0x00,                   /**< Just one bit used to indicate broadcast. */
    0xC0, 0xA8, 0x00, 0x0A, /**< Client's IP address if it has one or 0 is put in this field. */
    0x00, 0xAA, 0xAA, 0xAA, /**< The IP address that the server is assigning to the client. */
    0x00, 0xAA, 0xAA, 0xAA, /**< The DHCP server address that the client should use. */
    0x00, 0xAA, 0xAA, 0xAA  /**< Gateway IP address in case the server client are on different subnets. */
};

/*
 * IP-clash detection is currently only used internally. When DHCP doesn't respond, the
 * driver can try out a random LinkLayer IP address (169.254.x.x).  It will send out a
 * gratuitous ARP message and, after a period of time, check the variables here below:
 */
#if ( ipconfigARP_USE_CLASH_DETECTION != 0 )
    /* Becomes non-zero if another device responded to a gratuitous ARP message. */
    BaseType_t xARPHadIPClash;
    /* MAC-address of the other device containing the same IP-address. */
    MACAddress_t xARPClashMacAddress;
#endif /* ipconfigARP_USE_CLASH_DETECTION */


/** @brief For convenience, a MAC address of all 0xffs is defined const for quick
 * reference. */
const MACAddress_t xBroadcastMACAddress = { { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } };

/** @brief Structure that stores the netmask, gateway address and DNS server addresses. */
NetworkAddressingParameters_t xNetworkAddressing =
{
    0xC0C0C0C0, /* 192.192.192.192 - Default IP address. */
    0xFFFFFF00, /* 255.255.255.0 - Netmask. */
    0xC0C0C001, /* 192.192.192.1 - Gateway Address. */
    0x01020304, /* 1.2.3.4 - DNS server address. */
    0xC0C0C0FF
};              /* 192.192.192.255 - Broadcast address. */

/** @brief Structure that stores the netmask, gateway address and DNS server addresses. */
NetworkAddressingParameters_t xDefaultAddressing =
{
    0xC0C0C0C0, /* 192.192.192.192 - Default IP address. */
    0xFFFFFF00, /* 255.255.255.0 - Netmask. */
    0xC0C0C001, /* 192.192.192.1 - Gateway Address. */
    0x01020304, /* 1.2.3.4 - DNS server address. */
    0xC0C0C0FF
};

size_t xPortGetMinimumEverFreeHeapSize( void )
{
    return 0;
}


BaseType_t xApplicationDNSQueryHook_Multi( struct xNetworkEndPoint * pxEndPoint,
                                           const char * pcName )
{
}

StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                     StackType_t * pxEndOfStack,
                                     TaskFunction_t pxCode,
                                     void * pvParameters )
{
}

uint32_t ulApplicationGetNextSequenceNumber( uint32_t ulSourceAddress,
                                             uint16_t usSourcePort,
                                             uint32_t ulDestinationAddress,
                                             uint16_t usDestinationPort )
{
}
BaseType_t xNetworkInterfaceInitialise( void )
{
}
void vApplicationIPNetworkEventHook_Multi( eIPCallbackEvent_t eNetworkEvent,
                                           struct xNetworkEndPoint * pxEndPoint )
{
}
void vApplicationDaemonTaskStartupHook( void )
{
}
void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer,
                                     StackType_t ** ppxTimerTaskStackBuffer,
                                     configSTACK_DEPTH_TYPE * puxTimerTaskStackSize )
{
}
void vPortDeleteThread( void * pvTaskToDelete )
{
}
void vApplicationIdleHook( void )
{
}
void vApplicationTickHook( void )
{
}
unsigned long ulGetRunTimeCounterValue( void )
{
}
void vPortEndScheduler( void )
{
}
BaseType_t xPortStartScheduler( void )
{
}
void vPortEnterCritical( void )
{
}
void vPortExitCritical( void )
{
}

void * pvPortMalloc( size_t xWantedSize )
{
    return malloc( xWantedSize );
}

void vPortFree( void * pv )
{
    free( pv );
}

void vPortGenerateSimulatedInterrupt( uint32_t ulInterruptNumber )
{
}
void vPortCloseRunningThread( void * pvTaskToDelete,
                              volatile BaseType_t * pxPendYield )
{
}
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                    StackType_t ** ppxIdleTaskStackBuffer,
                                    configSTACK_DEPTH_TYPE * puxIdleTaskStackSize )
{
}
void vConfigureTimerForRunTimeStats( void )
{
}

/*--------------------------------STUB Functions---------------------------------------*/

eDHCPCallbackAnswer_t xStubApplicationDHCPHook_Multi( eDHCPCallbackPhase_t eDHCPPhase,
                                                      struct xNetworkEndPoint * pxEndPoint,
                                                      IP_Address_t * pxIPAddress,
                                                      int cmock_num_calls )
{
    TEST_ASSERT_EQUAL( eStubExpectedDHCPPhase, eDHCPPhase );
    TEST_ASSERT_EQUAL( pxStubExpectedEndPoint, pxEndPoint );
    TEST_ASSERT_EQUAL( ulStubExpectedIPAddress, pxIPAddress->ulIP_IPv4 );

    return eStubExpectedReturn;
}

BaseType_t NetworkInterfaceOutputFunction_Stub_Called = 0;

BaseType_t NetworkInterfaceOutputFunction_Stub( struct xNetworkInterface * pxDescriptor,
                                                NetworkBufferDescriptor_t * const pxNetworkBuffer,
                                                BaseType_t xReleaseAfterSend )
{
    NetworkInterfaceOutputFunction_Stub_Called++;
    return 0;
}


static NetworkBufferDescriptor_t * GetNetworkBuffer( size_t SizeOfEthBuf,
                                                     long unsigned int xTimeToBlock,
                                                     int callbacks )
{
    NetworkBufferDescriptor_t * pxNetworkBuffer = malloc( sizeof( NetworkBufferDescriptor_t ) + prvALIGNED_BUFFER_PADDING ) + prvALIGNED_BUFFER_PADDING;

    pxNetworkBuffer->pucEthernetBuffer = malloc( SizeOfEthBuf + prvALIGNED_BUFFER_PADDING ) + prvALIGNED_BUFFER_PADDING;

    /* Ignore the callback count. */
    ( void ) callbacks;
    /* Ignore the timeout. */
    ( void ) xTimeToBlock;

    /* Set the global network buffer so that memory can be freed later on. */
    pxGlobalNetworkBuffer[ GlobalBufferCounter++ ] = pxNetworkBuffer;

    return pxNetworkBuffer;
}

static void ReleaseNetworkBuffer( void )
{
    /* Free the ethernet buffer. */
    free( ( ( uint8_t * ) pxGlobalNetworkBuffer[ --GlobalBufferCounter ]->pucEthernetBuffer ) - prvALIGNED_BUFFER_PADDING );
    /* Free the network buffer. */
    free( ( ( uint8_t * ) pxGlobalNetworkBuffer[ GlobalBufferCounter ] ) - prvALIGNED_BUFFER_PADDING );
}

static void ReleaseUDPBuffer( const void * temp,
                              int callbacks )
{
    /* Should just call network buffer. */
    ReleaseNetworkBuffer();
}

static int32_t RecvFromStub( const ConstSocket_t xSocket,
                             void * pvBuffer,
                             size_t uxBufferLength,
                             BaseType_t xFlags,
                             struct freertos_sockaddr * pxSourceAddress,
                             socklen_t * pxSourceAddressLength,
                             int callbacks )
{
    switch( callbacks )
    {
        case 0:
            memset( pucUDPBuffer, 0, xSizeofUDPBuffer );
            break;

        case 1:
            /* Put in correct DHCP cookie. */
            ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulDHCPCookie = dhcpCOOKIE;
            /* Put incorrect DHCP opcode. */
            ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucOpcode = dhcpREPLY_OPCODE + 10;
            break;
    }

    if( xFlags == FREERTOS_ZERO_COPY )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;
    }

    return xSizeofUDPBuffer;
}


static int32_t FreeRTOS_recvfrom_Generic( const ConstSocket_t xSocket,
                                          void * pvBuffer,
                                          size_t uxBufferLength,
                                          BaseType_t xFlags,
                                          struct freertos_sockaddr * pxSourceAddress,
                                          socklen_t * pxSourceAddressLength,
                                          int callbacks )
{
    if( xFlags == FREERTOS_ZERO_COPY )
    {
        *( ( uint8_t ** ) pvBuffer ) = ucGenericPtr;
    }

    return ulGenericLength;
}

static int32_t FreeRTOS_recvfrom_Small_NullBuffer( const ConstSocket_t xSocket,
                                                   void * pvBuffer,
                                                   size_t uxBufferLength,
                                                   BaseType_t xFlags,
                                                   struct freertos_sockaddr * pxSourceAddress,
                                                   socklen_t * pxSourceAddressLength,
                                                   int callbacks )
{
    /* Admittedly, returning a (NULL, 1) slice is contrived, but coverage speaks. */
    *( ( uint8_t ** ) pvBuffer ) = NULL;
    return 1;
}

static int32_t FreeRTOS_recvfrom_eWaitingOfferRecvfromLessBytesNoTimeout( const ConstSocket_t xSocket,
                                                                          void * pvBuffer,
                                                                          size_t uxBufferLength,
                                                                          BaseType_t xFlags,
                                                                          struct freertos_sockaddr * pxSourceAddress,
                                                                          socklen_t * pxSourceAddressLength,
                                                                          int callbacks )
{
    if( xFlags == FREERTOS_ZERO_COPY )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;
    }

    memset( pucUDPBuffer, 0, xSizeofUDPBuffer );

    return sizeof( DHCPMessage_IPv4_t ) - 1;
}

static int32_t FreeRTOS_recvfrom_ResetAndIncorrectStateWithSocketAlreadyCreated_validUDPmessage( const ConstSocket_t xSocket,
                                                                                                 void * pvBuffer,
                                                                                                 size_t uxBufferLength,
                                                                                                 BaseType_t xFlags,
                                                                                                 struct freertos_sockaddr * pxSourceAddress,
                                                                                                 socklen_t * pxSourceAddressLength,
                                                                                                 int callbacks )
{
    NetworkEndPoint_t * pxIterator = pxNetworkEndPoints;

    while( pxIterator != NULL )
    {
        pxIterator->xDHCPData.xDHCPSocket = NULL;
        pxIterator = pxIterator->pxNext;
    }

    if( ( xFlags & FREERTOS_ZERO_COPY ) != 0 )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;
    }

    memset( pucUDPBuffer, 0, xSizeofUDPBuffer );
    /* Put in correct DHCP cookie. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulDHCPCookie = dhcpCOOKIE;
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucOpcode = dhcpREPLY_OPCODE;
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulTransactionID = FreeRTOS_htonl( 0x01ABCDEF );

    return xSizeofUDPBuffer;
}

static int32_t FreeRTOS_recvfrom_ResetAndIncorrectStateWithSocketAlreadyCreated_validUDPmessage_TwoFlagOptions_nullbytes( const ConstSocket_t xSocket,
                                                                                                                          void * pvBuffer,
                                                                                                                          size_t uxBufferLength,
                                                                                                                          BaseType_t xFlags,
                                                                                                                          struct freertos_sockaddr * pxSourceAddress,
                                                                                                                          socklen_t * pxSourceAddressLength,
                                                                                                                          int callbacks )
{
    int32_t lBytes = 0;
    NetworkEndPoint_t * pxIterator = pxNetworkEndPoints;

    while( pxIterator != NULL )
    {
        pxIterator->xDHCPData.xDHCPSocket = NULL;
        pxIterator = pxIterator->pxNext;
    }

    if( xFlags == FREERTOS_ZERO_COPY + FREERTOS_MSG_PEEK )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;

        memset( pucUDPBuffer, 0, xSizeofUDPBuffer );
        /* Put in correct DHCP cookie. */
        ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulDHCPCookie = dhcpCOOKIE;
        ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucOpcode = dhcpREPLY_OPCODE;
        ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulTransactionID = FreeRTOS_htonl( 0x01ABCDEF );

        lBytes = xSizeofUDPBuffer;
    }
    else
    {
        pvBuffer = NULL;
    }

    return lBytes;
}

static int32_t FreeRTOS_recvfrom_ResetAndIncorrectStateWithSocketAlreadyCreated_validUDPmessage_TwoFlagOptions_nullbuffer( const ConstSocket_t xSocket,
                                                                                                                           void * pvBuffer,
                                                                                                                           size_t uxBufferLength,
                                                                                                                           BaseType_t xFlags,
                                                                                                                           struct freertos_sockaddr * pxSourceAddress,
                                                                                                                           socklen_t * pxSourceAddressLength,
                                                                                                                           int callbacks )
{
    NetworkEndPoint_t * pxIterator = pxNetworkEndPoints;

    while( pxIterator != NULL )
    {
        pxIterator->xDHCPData.xDHCPSocket = NULL;
        pxIterator = pxIterator->pxNext;
    }

    if( xFlags == FREERTOS_ZERO_COPY + FREERTOS_MSG_PEEK )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;

        memset( pucUDPBuffer, 0, xSizeofUDPBuffer );
        /* Put in correct DHCP cookie. */
        ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulDHCPCookie = dhcpCOOKIE;
        ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucOpcode = dhcpREPLY_OPCODE;
        ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulTransactionID = FreeRTOS_htonl( 0x01ABCDEF );
    }
    else
    {
        *( ( uint8_t ** ) pvBuffer ) = NULL;
    }

    return xSizeofUDPBuffer;
}

static int32_t FreeRTOS_recvfrom_ResetAndIncorrectStateWithSocketAlreadyCreated_validUDPmessage_IncorrectDHCPCookie( const ConstSocket_t xSocket,
                                                                                                                     void * pvBuffer,
                                                                                                                     size_t uxBufferLength,
                                                                                                                     BaseType_t xFlags,
                                                                                                                     struct freertos_sockaddr * pxSourceAddress,
                                                                                                                     socklen_t * pxSourceAddressLength,
                                                                                                                     int callbacks )
{
    NetworkEndPoint_t * pxIterator = pxNetworkEndPoints;

    while( pxIterator != NULL )
    {
        pxIterator->xDHCPData.xDHCPSocket = NULL;
        pxIterator = pxIterator->pxNext;
    }

    if( xFlags == FREERTOS_ZERO_COPY + FREERTOS_MSG_PEEK )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;
    }

    memset( pucUDPBuffer, 0, xSizeofUDPBuffer );
    /* Put in correct DHCP cookie. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulDHCPCookie = dhcpBROADCAST;
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucOpcode = dhcpREPLY_OPCODE;
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulTransactionID = FreeRTOS_htonl( 0x01ABCDEF );

    return xSizeofUDPBuffer;
}

static int32_t FreeRTOS_recvfrom_ResetAndIncorrectStateWithSocketAlreadyCreated_validUDPmessage_IncorrectOpCode( const ConstSocket_t xSocket,
                                                                                                                 void * pvBuffer,
                                                                                                                 size_t uxBufferLength,
                                                                                                                 BaseType_t xFlags,
                                                                                                                 struct freertos_sockaddr * pxSourceAddress,
                                                                                                                 socklen_t * pxSourceAddressLength,
                                                                                                                 int callbacks )
{
    NetworkEndPoint_t * pxIterator = pxNetworkEndPoints;

    while( pxIterator != NULL )
    {
        pxIterator->xDHCPData.xDHCPSocket = NULL;
        pxIterator = pxIterator->pxNext;
    }

    if( xFlags == FREERTOS_ZERO_COPY + FREERTOS_MSG_PEEK )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;
    }

    memset( pucUDPBuffer, 0, xSizeofUDPBuffer );
    /* Put in correct DHCP cookie. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulDHCPCookie = dhcpCOOKIE;
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucOpcode = dhcpREQUEST_OPCODE;
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulTransactionID = FreeRTOS_htonl( 0x01ABCDEF );

    return xSizeofUDPBuffer;
}

static int32_t FreeRTOS_recvfrom_eWaitingOfferRecvfromSucceedsFalseCookieNoTimeout( const ConstSocket_t xSocket,
                                                                                    void * pvBuffer,
                                                                                    size_t uxBufferLength,
                                                                                    BaseType_t xFlags,
                                                                                    struct freertos_sockaddr * pxSourceAddress,
                                                                                    socklen_t * pxSourceAddressLength,
                                                                                    int callbacks )
{
    if( xFlags == FREERTOS_ZERO_COPY )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;
    }

    memset( pucUDPBuffer, 0, xSizeofUDPBuffer );
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulTransactionID = FreeRTOS_htonl( 0x01ABCDEF );

    return xSizeofUDPBuffer;
}

static int32_t FreeRTOS_recvfrom_eWaitingOfferRecvfromSucceedsFalseOpcodeNoTimeout( const ConstSocket_t xSocket,
                                                                                    void * pvBuffer,
                                                                                    size_t uxBufferLength,
                                                                                    BaseType_t xFlags,
                                                                                    struct freertos_sockaddr * pxSourceAddress,
                                                                                    socklen_t * pxSourceAddressLength,
                                                                                    int callbacks )
{
    if( xFlags == FREERTOS_ZERO_COPY )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;
    }

    memset( pucUDPBuffer, 0, xSizeofUDPBuffer );
    /* Put in correct DHCP cookie. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulDHCPCookie = dhcpCOOKIE;

    return xSizeofUDPBuffer;
}

static int32_t FreeRTOS_recvfrom_eWaitingOfferRecvfromSucceedsCorrectCookieAndOpcodeNoTimeout( const ConstSocket_t xSocket,
                                                                                               void * pvBuffer,
                                                                                               size_t uxBufferLength,
                                                                                               BaseType_t xFlags,
                                                                                               struct freertos_sockaddr * pxSourceAddress,
                                                                                               socklen_t * pxSourceAddressLength,
                                                                                               int callbacks )
{
    if( xFlags == FREERTOS_ZERO_COPY )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;
    }

    memset( pucUDPBuffer, 0, xSizeofUDPBuffer );
    /* Put in correct DHCP cookie. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulDHCPCookie = dhcpCOOKIE;
    /* Put in correct DHCP opcode. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucOpcode = dhcpREPLY_OPCODE;

    return xSizeofUDPBuffer;
}

static int32_t FreeRTOS_recvfrom_eWaitingOfferRecvfromSuccessCorrectTxID( const ConstSocket_t xSocket,
                                                                          void * pvBuffer,
                                                                          size_t uxBufferLength,
                                                                          BaseType_t xFlags,
                                                                          struct freertos_sockaddr * pxSourceAddress,
                                                                          socklen_t * pxSourceAddressLength,
                                                                          int callbacks )
{
    if( xFlags == FREERTOS_ZERO_COPY )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;
    }

    memset( pucUDPBuffer, 0, xSizeofUDPBuffer );
    /* Put in correct DHCP cookie. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulDHCPCookie = dhcpCOOKIE;
    /* Put in correct DHCP opcode. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucOpcode = dhcpREPLY_OPCODE;
    /* Put in correct DHCP Tx ID. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulTransactionID = FreeRTOS_htonl( xDHCPData.ulTransactionId );

    return xSizeofUDPBuffer;
}

static int32_t FreeRTOS_recvfrom_eWaitingOfferRecvfromSuccess_CorrectAddrType( const ConstSocket_t xSocket,
                                                                               void * pvBuffer,
                                                                               size_t uxBufferLength,
                                                                               BaseType_t xFlags,
                                                                               struct freertos_sockaddr * pxSourceAddress,
                                                                               socklen_t * pxSourceAddressLength,
                                                                               int callbacks )
{
    if( xFlags == FREERTOS_ZERO_COPY )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;
    }

    memset( pucUDPBuffer, 0, xSizeofUDPBuffer );
    /* Put in correct DHCP cookie. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulDHCPCookie = dhcpCOOKIE;
    /* Put in correct DHCP opcode. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucOpcode = dhcpREPLY_OPCODE;
    /* Put in correct DHCP Tx ID. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulTransactionID = FreeRTOS_htonl( xDHCPData.ulTransactionId );
    /* Put in address type as ethernet. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucAddressType = ( uint8_t ) dhcpADDRESS_TYPE_ETHERNET;

    return xSizeofUDPBuffer;
}

static int32_t FreeRTOS_recvfrom_eWaitingOfferRecvfromSuccess_CorrectAddrLen( const ConstSocket_t xSocket,
                                                                              void * pvBuffer,
                                                                              size_t uxBufferLength,
                                                                              BaseType_t xFlags,
                                                                              struct freertos_sockaddr * pxSourceAddress,
                                                                              socklen_t * pxSourceAddressLength,
                                                                              int callbacks )
{
    if( xFlags == FREERTOS_ZERO_COPY )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;
    }

    memset( pucUDPBuffer, 0, xSizeofUDPBuffer );
    /* Put in correct DHCP cookie. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulDHCPCookie = dhcpCOOKIE;
    /* Put in correct DHCP opcode. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucOpcode = dhcpREPLY_OPCODE;
    /* Put in correct DHCP Tx ID. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulTransactionID = FreeRTOS_htonl( xDHCPData.ulTransactionId );
    /* Put in address type as ethernet. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucAddressType = ( uint8_t ) dhcpADDRESS_TYPE_ETHERNET;
    /* Put in correct address length. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucAddressLength = ( uint8_t ) dhcpETHERNET_ADDRESS_LENGTH;
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulYourIPAddress_yiaddr = 0xFFFFFFFF;

    return xSizeofUDPBuffer;
}

static int32_t FreeRTOS_recvfrom_eWaitingOfferRecvfromSuccess_LocalHostAddr( const ConstSocket_t xSocket,
                                                                             void * pvBuffer,
                                                                             size_t uxBufferLength,
                                                                             BaseType_t xFlags,
                                                                             struct freertos_sockaddr * pxSourceAddress,
                                                                             socklen_t * pxSourceAddressLength,
                                                                             int callbacks )
{
    if( xFlags == FREERTOS_ZERO_COPY )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;
    }

    memset( pucUDPBuffer, 0, xSizeofUDPBuffer );
    /* Put in correct DHCP cookie. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulDHCPCookie = dhcpCOOKIE;
    /* Put in correct DHCP opcode. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucOpcode = dhcpREPLY_OPCODE;
    /* Put in correct DHCP Tx ID. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulTransactionID = FreeRTOS_htonl( xDHCPData.ulTransactionId );
    /* Put in address type as ethernet. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucAddressType = ( uint8_t ) dhcpADDRESS_TYPE_ETHERNET;
    /* Put in correct address length. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucAddressLength = ( uint8_t ) dhcpETHERNET_ADDRESS_LENGTH;
    /* Non local host address. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulYourIPAddress_yiaddr = 0x01FFFF7F;

    return xSizeofUDPBuffer;
}

static int32_t FreeRTOS_recvfrom_eWaitingOfferRecvfromSuccess_NonLocalHostAddr( const ConstSocket_t xSocket,
                                                                                void * pvBuffer,
                                                                                size_t uxBufferLength,
                                                                                BaseType_t xFlags,
                                                                                struct freertos_sockaddr * pxSourceAddress,
                                                                                socklen_t * pxSourceAddressLength,
                                                                                int callbacks )
{
    if( xFlags == FREERTOS_ZERO_COPY )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;
    }

    memset( pucUDPBuffer, 0, xSizeofUDPBuffer );
    /* Put in correct DHCP cookie. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulDHCPCookie = dhcpCOOKIE;
    /* Put in correct DHCP opcode. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucOpcode = dhcpREPLY_OPCODE;
    /* Put in correct DHCP Tx ID. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulTransactionID = FreeRTOS_htonl( xDHCPData.ulTransactionId + 1 );
    /* Put in address type as ethernet. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucAddressType = ( uint8_t ) dhcpADDRESS_TYPE_ETHERNET;
    /* Put in correct address length. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucAddressLength = ( uint8_t ) dhcpETHERNET_ADDRESS_LENGTH;
    /* Non local host address. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulYourIPAddress_yiaddr = 0x01FFFF3F;

    return xSizeofUDPBuffer;
}

static int32_t FreeRTOS_recvfrom_eWaitingOfferRecvfromSuccess_LocalMACAddrNotMatching( const ConstSocket_t xSocket,
                                                                                       void * pvBuffer,
                                                                                       size_t uxBufferLength,
                                                                                       BaseType_t xFlags,
                                                                                       struct freertos_sockaddr * pxSourceAddress,
                                                                                       socklen_t * pxSourceAddressLength,
                                                                                       int callbacks )
{
    if( xFlags == FREERTOS_ZERO_COPY )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;
    }

    memset( pucUDPBuffer, 0, xSizeofUDPBuffer );
    /* Put in correct DHCP cookie. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulDHCPCookie = dhcpCOOKIE;
    /* Put in correct DHCP opcode. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucOpcode = dhcpREPLY_OPCODE;
    /* Put in correct DHCP Tx ID. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulTransactionID = FreeRTOS_htonl( xDHCPData.ulTransactionId );
    /* Put in address type as ethernet. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucAddressType = ( uint8_t ) dhcpADDRESS_TYPE_ETHERNET;
    /* Put in correct address length. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucAddressLength = ( uint8_t ) dhcpETHERNET_ADDRESS_LENGTH;
    /* Non local host address. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulYourIPAddress_yiaddr = 0x01FFFF3F;

    return xSizeofUDPBuffer;
}

static int32_t FreeRTOS_recvfrom_LoopedCall( const ConstSocket_t xSocket,
                                             void * pvBuffer,
                                             size_t uxBufferLength,
                                             BaseType_t xFlags,
                                             struct freertos_sockaddr * pxSourceAddress,
                                             socklen_t * pxSourceAddressLength,
                                             int callbacks )
{
    NetworkEndPoint_t * pxIterator = pxNetworkEndPoints;
    size_t xSizeRetBufferSize = xSizeofUDPBuffer;

    if( callbacks == 2 )
    {
        pxNetworkEndPoints->xDHCPData.eDHCPState = eInitialWait;
    }
    else if( callbacks == 4 )
    {
        xSizeRetBufferSize = 200;
    }

    if( ( xFlags & FREERTOS_ZERO_COPY ) != 0 )
    {
        *( ( uint8_t ** ) pvBuffer ) = pucUDPBuffer;
    }

    memset( pucUDPBuffer, 0, xSizeofUDPBuffer );
    /* Put in correct DHCP cookie. */
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulDHCPCookie = dhcpCOOKIE;
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ucOpcode = dhcpREPLY_OPCODE;
    ( ( struct xDHCPMessage_IPv4 * ) pucUDPBuffer )->ulTransactionID = FreeRTOS_htonl( 0x01ABCDEF );

    return xSizeRetBufferSize;
}
/*-----------------------------------------------------------*/
