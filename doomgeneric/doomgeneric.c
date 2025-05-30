#include "m_argv.h"
#include "doomgeneric.h"

pixel_t* DG_ScreenBuffer;

void M_FindResponseFile(void);
void D_DoomMain (void);

void doomgeneric_Create(int argc, char **argv)
{
	// save arguments
    myargc = argc;
    myargv = argv;

	M_FindResponseFile();

	// [jnz] commented out, that is not generic
	// DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);

	DG_Init();

	D_DoomMain();
}

