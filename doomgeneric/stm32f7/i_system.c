//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//



#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdarg.h>

#include "config.h"

#include "deh_str.h"
#include "doomtype.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "i_joystick.h"
#include "i_sound.h"
#include "i_timer.h"
#include "i_video.h"

#include "i_system.h"

#include "w_wad.h"
#include "z_zone.h"

#define DEFAULT_RAM 6 /* MiB */

void I_AtExit(atexit_func_t func, boolean run_on_error)
{

}

// Tactile feedback function, probably used for the Logitech Cyberman
void I_Tactile(int on, int off, int total)
{

}

// Zone memory auto-allocation function that allocates the zone size
// by trying progressively smaller zone sizes until one is found that
// works.
// [jnz] For the microcontroller port this is just a fixed memory lump.
#warning "Move into linker script"
#define ZONE_MEM_ADDRESS     0xC02EE000
byte *I_ZoneBase (int *size)
{
    uint32_t zonemem = ZONE_MEM_ADDRESS;
    *size = DEFAULT_RAM * 1024 * 1024;
    return (byte*) zonemem;
}

void I_PrintBanner(char *msg)
{

}

void I_PrintDivider(void)
{

}

void I_PrintStartupBanner(char *gamedescription)
{

}

//
// I_ConsoleStdout
//
// Returns true if stdout is a real console, false if it is a file
//

boolean I_ConsoleStdout(void)
{
    return 0;
}

//
// I_Init
//
/*
void I_Init (void)
{
    I_CheckIsScreensaver();
    I_InitTimer();
    I_InitJoystick();
}
void I_BindVariables(void)
{
    I_BindVideoVariables();
    I_BindJoystickVariables();
    I_BindSoundVariables();
}
*/

//
// I_Quit
//

void I_Quit (void)
{

}

//
// I_Error
//

#ifndef STM32F769xx
static boolean already_quitting = false;
#endif

void I_Error (char *error, ...)
{
    while(1) {}
}

boolean I_GetMemoryValue(unsigned int offset, void *value, int size)
{
    while(1) {}
    return false;
}

