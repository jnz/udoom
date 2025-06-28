// DESCRIPTION:
//     Networking module which uses UART
//

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

//
// NETWORKING
//
static boolean g_init = true;
static int port = 0;
static net_addr_t uart_addr;
extern net_module_t net_sdl_module;

// --- UART CONFIG ---
UART_HandleTypeDef huart6;

static uint16_t CRC16_Calculate(const uint8_t *data, uint16_t length);

typedef enum {
    UART_SYNC1,
    UART_SYNC2,
    UART_LEN1,
    UART_LEN2,
    UART_PAYLOAD,
    UART_CRC1,
    UART_CRC2
} uart_rx_state_t;

static uart_rx_state_t uart_rx_state = UART_SYNC1;
static uint16_t uart_rx_len = 0;
static uint16_t uart_rx_index = 0;
static uint8_t uart_rx_buffer[256];
static uint16_t uart_rx_crc_calc = 0;
static uint16_t uart_rx_crc_recv = 0;

void MX_USART6_UART_Init(void)
{
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
        I_Error("Failed to init UART");
    }
}

static bool UART_Parser(uint8_t byte, net_packet_t **packet)
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

                uart_rx_state = UART_SYNC1;
                return true;
            }
            else
            {
                // CRC Fehler
                uart_rx_state = UART_SYNC1;
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
}

static boolean NET_SDL_RecvPacket(net_addr_t **addr, net_packet_t **packet)
{
    int result = 0;

    uint8_t byte;
    while (HAL_UART_Receive(&huart6, &byte, 1, 10) == HAL_OK)
    {
        if (UART_Parser(byte, packet))
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
