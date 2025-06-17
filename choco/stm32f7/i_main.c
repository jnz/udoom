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
//	Main program, simply calls D_DoomMain high level loop.
//

#include "config.h"

#include <stdio.h>

#include "doomtype.h"
#include "i_system.h"
#include "m_argv.h"

// Stub for time() -> FIXME: who calls this?
#include <time.h>
time_t time(time_t *t)
{
    if (t) *t = 0;
    return 0;
}

//
// D_DoomMain()
// Not a globally visible function, just included for source reference,
// calls all startup code, parses command line options.
//

void D_DoomMain (void);

#ifdef FEATURE_MULTIPLAYER
void NET_WaitForLaunch(void)
{
    while(net_waiting_for_launch)
    {
        NET_CL_Run();
        NET_SV_Run();

        if (!net_client_connected)
        {
            NVIC_SystemReset();
        }
    }

}
#endif

int doom_main(int argc, char **argv)
{
    // save arguments

    myargc = argc;
    myargv = argv;

    // M_FindResponseFile();

    // start doom

    D_DoomMain ();

    return 0;
}

