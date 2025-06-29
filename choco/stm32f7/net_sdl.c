// DESCRIPTION:
//     Networking module which uses UART
//

/******************************************************************************
 * INCLUDE FILES
 ******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "doomtype.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "net_defs.h"
#include "net_io.h"
#include "net_packet.h"
#include "z_zone.h"

#include "stm32f7xx_hal.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define UART_RX_DMA_SIZE 1024
#define UART_RX_SIZE     256

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

typedef enum {
    UART_SYNC1,
    UART_SYNC2,
    UART_LEN1,
    UART_LEN2,
    UART_PAYLOAD,
    UART_CRC1,
    UART_CRC2
} uart_rx_state_t;

/******************************************************************************
 * GLOBAL DATA DEFINITIONS
 ******************************************************************************/

extern net_module_t net_sdl_module; // defined later in this file

UART_HandleTypeDef huart6;
DMA_HandleTypeDef hdma_usart6_rx;

/******************************************************************************
 * LOCAL DATA DEFINITIONS
 ******************************************************************************/

static boolean g_init = false;
static int port = 0;
static net_addr_t uart_addr;

static uart_rx_state_t uart_rx_state = UART_SYNC1;
static uint16_t uart_rx_len = 0;
static uint16_t uart_rx_index = 0;
static uint8_t uart_rx_buffer[UART_RX_SIZE];
static uint16_t uart_rx_crc_calc = 0;
static uint16_t uart_rx_crc_recv = 0;

/* UART ring buffer */
static uint8_t uart_rx_dma_buffer[UART_RX_DMA_SIZE];
static volatile uint16_t uart_rx_dma_last_pos = 0;

/******************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 ******************************************************************************/

static uint16_t CRC16_Calculate(const uint8_t *data, uint16_t length);
static uint16_t UART_DMA_GetCurrentPos(void);
static uint16_t UART_DMA_Read(uint8_t *dest, uint16_t maxlen);

/******************************************************************************
 * FUNCTION PROTOTYPES
 ******************************************************************************/

/******************************************************************************
 * FUNCTION BODIES
 ******************************************************************************/

void MX_DMA_Init(void)
{
    __HAL_RCC_DMA2_CLK_ENABLE();

    hdma_usart6_rx.Instance                 = DMA2_Stream1;
    hdma_usart6_rx.Init.Channel             = DMA_CHANNEL_5; // USART6_RX
    hdma_usart6_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart6_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart6_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart6_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart6_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart6_rx.Init.Mode                = DMA_CIRCULAR; // Important
    hdma_usart6_rx.Init.Priority            = DMA_PRIORITY_LOW;
    hdma_usart6_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_usart6_rx) != HAL_OK)
    {
        I_Error("DMA Init Error");
    }

    // DMA IRQ
    HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
}

void MX_USART6_UART_Init(void)
{
    MX_DMA_Init();

    huart6.Instance = USART6;
    huart6.Init.BaudRate = 115200;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) != HAL_OK)
    {
        I_Error("UART6 init failed");
    }

    __HAL_LINKDMA(&huart6, hdmarx, hdma_usart6_rx);

    // DMA Circular RX start:
    if (HAL_UART_Receive_DMA(&huart6, uart_rx_dma_buffer, UART_RX_DMA_SIZE) != HAL_OK)
    {
        I_Error("UART6 DMA receive error");
    }
}

static uint16_t UART_DMA_GetCurrentPos(void)
{
    return (UART_RX_DMA_SIZE - __HAL_DMA_GET_COUNTER(huart6.hdmarx));
}

static uint16_t UART_DMA_Read(uint8_t *dest, uint16_t maxlen)
{
    uint16_t pos = UART_DMA_GetCurrentPos();
    uint16_t len = 0;

    if (pos == uart_rx_dma_last_pos)
        return 0;

    if (pos > uart_rx_dma_last_pos)
    {
        len = pos - uart_rx_dma_last_pos;
        if (len > maxlen)
            len = maxlen;

        memcpy(dest, &uart_rx_dma_buffer[uart_rx_dma_last_pos], len);
    }
    else
    {
        // Ring wrap around
        len = UART_RX_DMA_SIZE - uart_rx_dma_last_pos;
        if (len > maxlen)
            len = maxlen;

        memcpy(dest, &uart_rx_dma_buffer[uart_rx_dma_last_pos], len);

        if (len < maxlen && pos > 0)
        {
            uint16_t len2 = pos < (maxlen - len) ? pos : (maxlen - len);
            memcpy(dest + len, uart_rx_dma_buffer, len2);
            len += len2;
        }
    }

    uart_rx_dma_last_pos = (uart_rx_dma_last_pos + len) % UART_RX_DMA_SIZE;

    return len;
}

static boolean UART_Parser(uint8_t byte, net_packet_t **packet)
{
    switch (uart_rx_state)
    {
        case UART_SYNC1:
            if (byte == 0x06)
                uart_rx_state = UART_SYNC2;
            break;

        case UART_SYNC2:
            if (byte == 0x66)
                uart_rx_state = UART_LEN1;
            else
                uart_rx_state = UART_SYNC1;
            break;

        case UART_LEN1:
            uart_rx_len = byte;
            uart_rx_state = UART_LEN2;
            break;

        case UART_LEN2:
            uart_rx_len |= (byte << 8);
            if (uart_rx_len == 0 || uart_rx_len > sizeof(uart_rx_buffer))
            {
                uart_rx_state = UART_SYNC1; // reset
            }
            else
            {
                uart_rx_index = 0;
                uart_rx_state = UART_PAYLOAD;
            }
            break;

        case UART_PAYLOAD:
            uart_rx_buffer[uart_rx_index++] = byte;
            if (uart_rx_index >= uart_rx_len)
                uart_rx_state = UART_CRC1;
            break;

        case UART_CRC1:
            uart_rx_crc_recv = byte;
            uart_rx_state = UART_CRC2;
            break;

        case UART_CRC2:
            uart_rx_crc_recv |= (byte << 8);
            uart_rx_crc_calc = CRC16_Calculate((uint8_t *)"\x06\x66", 2);
            uart_rx_crc_calc = CRC16_Calculate((uint8_t *)&uart_rx_len, 2);
            uart_rx_crc_calc = CRC16_Calculate(uart_rx_buffer, uart_rx_len);

            if (uart_rx_crc_calc == uart_rx_crc_recv)
            {
                *packet = NET_NewPacket(uart_rx_len);
                memcpy((*packet)->data, uart_rx_buffer, uart_rx_len);
                (*packet)->len = uart_rx_len;

                printf("Received packet: %i bytes (crc: %i)\n", (*packet)->len, uart_rx_crc_calc);
                uart_rx_state = UART_SYNC1;
                return true;
            }
            else
            {
                // CRC Fehler
                uart_rx_state = UART_SYNC1;
                printf("CRC error\n");
            }
            break;

        default:
            uart_rx_state = UART_SYNC1;
            break;
    }

    return false;
}

static boolean NET_SDL_InitClient(void)
{
    int p;

    if (g_init)
        return true;

    p = M_CheckParmWithArgs("-port", 1);
    if (p > 0)
        port = atoi(myargv[p+1]);

    MX_USART6_UART_Init();
    uart_addr.handle = &huart6;
    uart_addr.module = &net_sdl_module;

    g_init = true;

    return true;
}

static boolean NET_SDL_InitServer(void)
{
    return NET_SDL_InitClient();
}

static void NET_SDL_SendPacket(net_addr_t *addr, net_packet_t *packet)
{
    uint8_t header[4] = { 0x06, 0x66, packet->len & 0xFF, (packet->len >> 8) & 0xFF };
    uint16_t crc = CRC16_Calculate(header, 4);
    crc = CRC16_Calculate(packet->data, packet->len);

    HAL_UART_Transmit(&huart6, header, 4, HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart6, packet->data, packet->len, HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart6, (uint8_t *)&crc, 2, HAL_MAX_DELAY);
    printf("Send packet: %i bytes (crc: %i)\n", packet->len, crc);
}

static boolean NET_SDL_RecvPacket(net_addr_t **addr, net_packet_t **packet)
{
    uint8_t rx_chunk[256];
    uint16_t received = UART_DMA_Read(rx_chunk, sizeof(rx_chunk));

    if (received == 0)
        return false;

    for (uint16_t i = 0; i < received; i++)
    {
        if (UART_Parser(rx_chunk[i], packet))
        {
            *addr = &uart_addr;
            return true;
        }
    }

    return false;
}

void NET_SDL_AddrToString(net_addr_t *addr, char *buffer, int buffer_len)
{
    M_snprintf(buffer, buffer_len, "UART");
}

static void NET_SDL_FreeAddress(net_addr_t *addr)
{
    (void)addr; // unused parameter
}

net_addr_t *NET_SDL_ResolveAddress(char *address)
{
    return &uart_addr;
}

// Complete module

net_module_t net_sdl_module =
{
    NET_SDL_InitClient,
    NET_SDL_InitServer,
    NET_SDL_SendPacket,
    NET_SDL_RecvPacket,
    NET_SDL_AddrToString,
    NET_SDL_FreeAddress,
    NET_SDL_ResolveAddress,
};

static uint16_t CRC16_Calculate(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0x0000;
    for (uint16_t i = 0; i < length; i++)
    {
        crc ^= (uint16_t)(data[i]) << 8;
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}
