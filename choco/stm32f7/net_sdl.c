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

//
// NETWORKING
//
static boolean g_init = true;
static int port = 0;
static net_addr_t uart_addr;

extern net_module_t net_sdl_module;

static boolean NET_SDL_InitClient(void)
{
    int p;

    if (g_init)
        return true;

    p = M_CheckParmWithArgs("-port", 1);
    if (p > 0)
        port = atoi(myargv[p+1]);

    uart_addr.module = &net_sdl_module;
    uart_addr.handle = NULL;

    g_init = true;

    return true;
}

static boolean NET_SDL_InitServer(void)
{
    return NET_SDL_InitClient();
}

static void NET_SDL_SendPacket(net_addr_t *addr, net_packet_t *packet)
{
    /*
    sdl_packet.channel = 0;
    sdl_packet.data = packet->data;
    sdl_packet.len = packet->len;
    sdl_packet.address = ip;
    */
}

static boolean NET_SDL_RecvPacket(net_addr_t **addr, net_packet_t **packet)
{
    int result = 0;

    // result = readuart(...);

    // no packets received

    if (result == 0)
        return false;

    // Put the data into a new packet structure

    /*
    *packet = NET_NewPacket(recvpacket->len);
    memcpy((*packet)->data, recvpacket->data, recvpacket->len);
    (*packet)->len = recvpacket->len;
    */

    // Address

    *addr = &uart_addr;

    return true;
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

