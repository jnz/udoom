/*
              | |
     _   _  __| | ___   ___  _ __ ___
    | | | |/ _` |/ _ \ / _ \| '_ ` _ \
    | |_| | (_| | (_) | (_) | | | | | |
     \__,_|\__,_|\___/ \___/|_| |_| |_|

   Doom for the STM32F7 microcontroller
*/

#include "config.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_event.h"
#include "d_main.h"
#include "i_video.h"
#include "z_zone.h"
#include "tables.h"
#include "doomkeys.h"
#include "doomgeneric.h"

#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>

#ifdef STM32F769xx
#include "stm32f769i_discovery_lcd.h"
#define DMA2D_HW_ACCEL
#else
#include "stm32f7508_discovery_lcd.h"
#define DMA2D_HW_ACCEL
#endif

//#define CMAP256

struct FB_BitField
{
    uint32_t offset;            /* beginning of bitfield    */
    uint32_t length;            /* length of bitfield       */
};

struct FB_ScreenInfo
{
    uint32_t xres;              /* visible resolution       */
    uint32_t yres;
    uint32_t xres_virtual;      /* virtual resolution       */
    uint32_t yres_virtual;

    uint32_t bits_per_pixel;

    struct FB_BitField red;     /* bitfield in s_Fb mem if true color, */
    struct FB_BitField green;   /* else only length is significant */
    struct FB_BitField blue;
    struct FB_BitField transp;  /* transparency         */
};

static struct FB_ScreenInfo s_Fb;
int fb_scaling = 1;
int usemouse = 0;

#ifdef DMA2D_HW_ACCEL

void HAL_DMA2D_MspInit(DMA2D_HandleTypeDef *hdma2d);
void HAL_DMA2D_MspDeInit(DMA2D_HandleTypeDef *hdma2d);

static uint32_t dma2d_clut[256]; // palette for STM's DMA2D hardware acceleration
#ifdef STM32F769xx
#define DMA2D_HW_ACCEL_SCALE_2X  // if enabled: scale up the framebuffer 2x
#endif
/*
    Run Python script python/precompute_gamma.py
    Table for 0.5 gamma correction
 */
const uint8_t gamma_lut[256] = {
      0,  16,  23,  28,  32,  36,  39,  42,  45,  48,  50,  53,  55,  58,  60,  62,
     64,  66,  68,  70,  71,  73,  75,  77,  78,  80,  81,  83,  84,  86,  87,  89,
     90,  92,  93,  94,  96,  97,  98, 100, 101, 102, 103, 105, 106, 107, 108, 109,
    111, 112, 113, 114, 115, 116, 117, 118, 119, 121, 122, 123, 124, 125, 126, 127,
    128, 129, 130, 131, 132, 133, 134, 135, 135, 136, 137, 138, 139, 140, 141, 142,
    143, 144, 145, 145, 146, 147, 148, 149, 150, 151, 151, 152, 153, 154, 155, 156,
    156, 157, 158, 159, 160, 160, 161, 162, 163, 164, 164, 165, 166, 167, 167, 168,
    169, 170, 170, 171, 172, 173, 173, 174, 175, 176, 176, 177, 178, 179, 179, 180,
    181, 181, 182, 183, 183, 184, 185, 186, 186, 187, 188, 188, 189, 190, 190, 191,
    192, 192, 193, 194, 194, 195, 196, 196, 197, 198, 198, 199, 199, 200, 201, 201,
    202, 203, 203, 204, 204, 205, 206, 206, 207, 208, 208, 209, 209, 210, 211, 211,
    212, 212, 213, 214, 214, 215, 215, 216, 217, 217, 218, 218, 219, 220, 220, 221,
    221, 222, 222, 223, 224, 224, 225, 225, 226, 226, 227, 228, 228, 229, 229, 230,
    230, 231, 231, 232, 233, 233, 234, 234, 235, 235, 236, 236, 237, 237, 238, 238,
    239, 240, 240, 241, 241, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 247,
    247, 248, 248, 249, 249, 250, 250, 251, 251, 252, 252, 253, 253, 254, 254, 255,
};
#ifdef DMA2D_HW_ACCEL_SCALE_2X
static byte* VideoBuffer2X;
#endif

#else

    #ifdef CMAP256

    boolean palette_changed;
    struct color colors[256];

    #else  // CMAP256

    static struct color colors[256];

    #endif  // CMAP256

#endif // DMA2D_HW_ACCEL

void I_GetEvent(void);

// The screen buffer; this is modified to draw things to the screen

#ifdef STM32F769xx
byte VideoBuffer[SCREENWIDTH * SCREENHEIGHT];
byte *I_VideoBuffer = VideoBuffer;
#else
byte *I_VideoBuffer;
#endif

// If true, game is running as a screensaver

boolean screensaver_mode = false;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen

boolean screenvisible;

// Mouse acceleration
//
// This emulates some of the behavior of DOS mouse drivers by increasing
// the speed when the mouse is moved fast.
//
// The mouse input values are input directly to the game, but when
// the values exceed the value of mouse_threshold, they are multiplied
// by mouse_acceleration to increase the speed.

float mouse_acceleration = 2.0;
int mouse_threshold = 10;

// Gamma correction level to use

int usegamma = 0;

typedef struct
{
    byte r;
    byte g;
    byte b;
} col_t;

// Palette converted to RGB565

static uint16_t rgb565_palette[256];

#ifdef DMA2D_HW_ACCEL
DMA2D_HandleTypeDef hdma2d;
void DMA2D_Init(void)
{
    hdma2d.Instance = DMA2D;
    hdma2d.Init.Mode = DMA2D_M2M_PFC;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_ARGB8888;
#ifdef DMA2D_HW_ACCEL_SCALE_2X
    hdma2d.Init.OutputOffset = BSP_LCD_GetXSize() - 2*SCREENWIDTH;
#else
    hdma2d.Init.OutputOffset = BSP_LCD_GetXSize() - SCREENWIDTH;
#endif

    hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_L8;
    hdma2d.LayerCfg[1].InputOffset = 0;
    hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hdma2d.LayerCfg[1].InputAlpha = 0xFF;

    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_ConfigLayer(&hdma2d, 1);
#ifdef DMA2D_HW_ACCEL_SCALE_2X
    // To scale up by 2x a temp. buffer is required.
    VideoBuffer2X = Z_Malloc (4 * SCREENWIDTH * SCREENHEIGHT, PU_STATIC, NULL);
#endif
}

void HAL_DMA2D_MspInit(DMA2D_HandleTypeDef *hdma2d)
{
    __HAL_RCC_DMA2D_CLK_ENABLE();
    HAL_NVIC_SetPriority(DMA2D_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2D_IRQn);
}

void HAL_DMA2D_MspDeInit(DMA2D_HandleTypeDef *hdma2d)
{
    __HAL_RCC_DMA2D_FORCE_RESET();
    __HAL_RCC_DMA2D_RELEASE_RESET();
}

#endif

#ifndef DMA2D_HW_ACCEL
void cmap_to_rgb565(uint16_t * out, uint8_t * in, int in_pixels)
{
    int i, j;
    struct color c;
    uint16_t r, g, b;

    for (i = 0; i < in_pixels; i++)
    {
        c = colors[*in];
        r = ((uint16_t)(c.r >> 3)) << 11;
        g = ((uint16_t)(c.g >> 2)) << 5;
        b = ((uint16_t)(c.b >> 3)) << 0;
        *out = (r | g | b);

        in++;
        for (j = 0; j < fb_scaling; j++) {
            out++;
        }
    }
}

void cmap_to_fb(uint8_t * out, uint8_t * in, int in_pixels)
{
    int i, j, k;
    struct color c;
    uint32_t pix;
    uint16_t r, g, b;

    for (i = 0; i < in_pixels; i++)
    {
        c = colors[*in];  /* R:8 G:8 B:8 format! */
        r = (uint16_t)(c.r >> (8 - s_Fb.red.length));
        g = (uint16_t)(c.g >> (8 - s_Fb.green.length));
        b = (uint16_t)(c.b >> (8 - s_Fb.blue.length));
        pix = r << s_Fb.red.offset;
        pix |= g << s_Fb.green.offset;
        pix |= b << s_Fb.blue.offset;
        pix |= 0xFF << s_Fb.transp.offset; // 0xFF for opaque

        for (k = 0; k < fb_scaling; k++) {
            for (j = 0; j < s_Fb.bits_per_pixel/8; j++) {
                *out = (pix >> (j*8));
                out++;
            }
        }
        in++;
    }
}
#endif

void I_InitGraphics (void)
{
    int i;

#ifdef DMA2D_HW_ACCEL
    DMA2D_Init();
#endif

    memset(&s_Fb, 0, sizeof(struct FB_ScreenInfo));

    s_Fb.xres = BSP_LCD_GetXSize();
    s_Fb.yres = BSP_LCD_GetYSize();
    s_Fb.xres_virtual = s_Fb.xres;
    s_Fb.yres_virtual = s_Fb.yres;

#ifdef CMAP256

    s_Fb.bits_per_pixel = 8;

#else  // CMAP256

    s_Fb.bits_per_pixel = 32;

    s_Fb.blue.length = 8;
    s_Fb.green.length = 8;
    s_Fb.red.length = 8;
    s_Fb.transp.length = 8;

    s_Fb.blue.offset = 0;
    s_Fb.green.offset = 8;
    s_Fb.red.offset = 16;
    s_Fb.transp.offset = 24;

#endif  // CMAP256

    printf("I_InitGraphics: framebuffer: x_res: %u, y_res: %u, x_virtual: %d, y_virtual: %d, bpp: %d\n",
            s_Fb.xres, s_Fb.yres, s_Fb.xres_virtual, s_Fb.yres_virtual, s_Fb.bits_per_pixel);

    printf("I_InitGraphics: framebuffer: RGBA: %d%d%d%d, red_off: %d, green_off: %d, blue_off: %d, transp_off: %d\n",
            s_Fb.red.length, s_Fb.green.length, s_Fb.blue.length, s_Fb.transp.length, s_Fb.red.offset, s_Fb.green.offset, s_Fb.blue.offset, s_Fb.transp.offset);

    printf("I_InitGraphics: DOOM screen size: w x h: %d x %d\n", SCREENWIDTH, SCREENHEIGHT);


    i = M_CheckParmWithArgs("-scaling", 1);
    if (i > 0) {
        i = atoi(myargv[i + 1]);
        fb_scaling = i;
        printf("I_InitGraphics: Scaling factor: %d\n", fb_scaling);
    } else {
        fb_scaling = s_Fb.xres / SCREENWIDTH;
        if (s_Fb.yres / SCREENHEIGHT < fb_scaling)
            fb_scaling = s_Fb.yres / SCREENHEIGHT;
        printf("I_InitGraphics: Auto-scaling factor: %d\n", fb_scaling);
    }


    /* Allocate screen to draw to */
#ifndef STM32F769xx
    I_VideoBuffer = (byte*)Z_Malloc (SCREENWIDTH * SCREENHEIGHT, PU_STATIC, NULL);  // For DOOM to draw on
#endif

    screenvisible = true;

    extern void I_InitInput(void);
    I_InitInput();
}

void I_ShutdownGraphics (void)
{
    // Z_Free (I_VideoBuffer);
}

void I_StartFrame (void)
{

}

void I_StartTic (void)
{
    I_GetEvent();
}

void I_UpdateNoBlit (void)
{
}

//
// I_FinishUpdate
//

#ifdef DMA2D_HW_ACCEL

#ifdef DMA2D_HW_ACCEL_SCALE_2X
static void scale2x(const uint8_t* src, uint8_t* dest, int w, int h)
{
    for (int y = 0; y < h; ++y) {
        const uint8_t* src_row = src + y * w;
        uint8_t* dst_row0 = dest + (y * 2) * (w * 2);
        uint8_t* dst_row1 = dst_row0 + (w * 2);

        for (int x = 0; x < w; ++x) {
            uint8_t p = src_row[x];
            dst_row0[x * 2 + 0] = p;
            dst_row0[x * 2 + 1] = p;
            dst_row1[x * 2 + 0] = p;
            dst_row1[x * 2 + 1] = p;
        }
    }
}
#endif

static void BlitDoomFrame(const uint8_t *src, uint32_t *dst, int width, int height)
{
#ifdef DMA2D_HW_ACCEL_SCALE_2X
    scale2x(src, VideoBuffer2X, width, height);
    uint8_t* src_scaled = VideoBuffer2X;
    const int scale = 2;
#else
    uint8_t* src_scaled = src;
    const int scale = 1;
#endif

    uint32_t* dst_center = dst +
        ((BSP_LCD_GetYSize() - scale*height) / 2) * BSP_LCD_GetXSize() +
        ((BSP_LCD_GetXSize() - scale*width ) / 2);

    // If the SDRAM memory is not up-to-date, the DMA2D copy will
    // create weird artifacts
    SCB_CleanDCache_by_Addr((uint32_t*)src_scaled, scale * scale * width * height);
    HAL_DMA2D_Start_IT(&hdma2d,
                    (uint32_t)src_scaled,
                    (uint32_t)dst_center,
                    scale*width,
                    scale*height);
    // sleep until dma2d transfer complete ISR gets called:
    while (HAL_DMA2D_GetState(&hdma2d) != HAL_DMA2D_STATE_READY)
    {
        __WFI();
    }
}
#endif // DMA2D_HW_ACCEL


void I_FinishUpdate (void)
{
#ifdef DMA2D_HW_ACCEL
    BlitDoomFrame(I_VideoBuffer, (uint32_t *)DG_ScreenBuffer, SCREENWIDTH, SCREENHEIGHT);
#else
    int y;
    int x_offset, x_offset_end;
    // int y_offset;
    unsigned char *line_in, *line_out;

    /* Offsets in case FB is bigger than DOOM */
    /* 600 = s_Fb heigt, 200 screenheight */
    /* 600 = s_Fb heigt, 200 screenheight */
    /* 2048 =s_Fb width, 320 screenwidth */
    // y_offset     = (((s_Fb.yres - (SCREENHEIGHT * fb_scaling)) * s_Fb.bits_per_pixel/8)) / 2;
    x_offset     = (((s_Fb.xres - (SCREENWIDTH  * fb_scaling)) * s_Fb.bits_per_pixel/8)) / 2; // XXX: siglent FB hack: /4 instead of /2, since it seems to handle the resolution in a funny way
    //x_offset     = 0;
    x_offset_end = ((s_Fb.xres - (SCREENWIDTH  * fb_scaling)) * s_Fb.bits_per_pixel/8) - x_offset;

    /* DRAW SCREEN */
    line_in  = (unsigned char *) I_VideoBuffer;
    line_out = (unsigned char *) DG_ScreenBuffer;

    y = SCREENHEIGHT;

    while (y--)
    {
        int i;
        for (i = 0; i < fb_scaling; i++) {
            line_out += x_offset;
#ifdef CMAP256
            if (fb_scaling == 1) {
                memcpy(line_out, line_in, SCREENWIDTH); /* fb_width is bigger than Doom SCREENWIDTH... */
            } else {
                int j;

                for (j = 0; j < SCREENWIDTH; j++) {
                    int k;
                    for (k = 0; k < fb_scaling; k++) {
                        line_out[j * fb_scaling + k] = line_in[j];
                    }
                }
            }
#else
            //cmap_to_rgb565((void*)line_out, (void*)line_in, SCREENWIDTH);
            cmap_to_fb((void*)line_out, (void*)line_in, SCREENWIDTH);
#endif
            line_out += (SCREENWIDTH * fb_scaling * (s_Fb.bits_per_pixel/8)) + x_offset_end;
        }
        line_in += SCREENWIDTH;
    }
#endif

    DG_DrawFrame();
}

//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
    memcpy (scr, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT);
}

//
// I_SetPalette
//
#define GFX_RGB565(r, g, b)         ((((r & 0xF8) >> 3) << 11) | (((g & 0xFC) >> 2) << 5) | ((b & 0xF8) >> 3))
#define GFX_RGB565_R(color)         ((0xF800 & color) >> 11)
#define GFX_RGB565_G(color)         ((0x07E0 & color) >> 5)
#define GFX_RGB565_B(color)         (0x001F & color)

void I_SetPalette (byte* palette)
{
#ifdef DMA2D_HW_ACCEL
    for (int i = 0; i < 256; ++i)
    {
        uint8_t r = gammatable[usegamma][*palette++];
        uint8_t g = gammatable[usegamma][*palette++];
        uint8_t b = gammatable[usegamma][*palette++];

        r = gamma_lut[r];
        g = gamma_lut[g];
        b = gamma_lut[b];

        dma2d_clut[i] = (0xFF << 24) | (r << 16) | (g << 8) | b;
    }
    DMA2D_CLUTCfgTypeDef clut_cfg = {
        .pCLUT = dma2d_clut,
        .CLUTColorMode = DMA2D_CCM_ARGB8888,
        .Size = 255
    };
    // Flush the cache before we upload the palette memory via DMA2D
    SCB_CleanDCache_by_Addr((uint32_t*)dma2d_clut, sizeof(dma2d_clut));
    HAL_DMA2D_CLUTLoad(&hdma2d, clut_cfg, 1);
    HAL_DMA2D_PollForTransfer(&hdma2d, HAL_MAX_DELAY);
#else
    for (int i=0; i<256; ++i ) {
        colors[i].a = 0;
        colors[i].r = gammatable[usegamma][*palette++];
        colors[i].g = gammatable[usegamma][*palette++];
        colors[i].b = gammatable[usegamma][*palette++];
    }
#endif

#ifdef CMAP256

    palette_changed = true;

#endif  // CMAP256
}


    // Optional: load into DMA2D right here (blocking)

// Given an RGB value, find the closest matching palette index.

int I_GetPaletteIndex (int r, int g, int b)
{
    int best, best_diff, diff;
    int i;
    col_t color;

    printf("I_GetPaletteIndex\n");

    best = 0;
    best_diff = INT_MAX;

    for (i = 0; i < 256; ++i)
    {
        color.r = GFX_RGB565_R(rgb565_palette[i]);
        color.g = GFX_RGB565_G(rgb565_palette[i]);
        color.b = GFX_RGB565_B(rgb565_palette[i]);

        diff = (r - color.r) * (r - color.r)
             + (g - color.g) * (g - color.g)
             + (b - color.b) * (b - color.b);

        if (diff < best_diff)
        {
            best = i;
            best_diff = diff;
        }

        if (diff == 0)
        {
            break;
        }
    }

    return best;
}

void I_BeginRead (void)
{
}

void I_EndRead (void)
{
}

void I_SetWindowTitle (char *title)
{
    DG_SetWindowTitle(title);
}

void I_GraphicsCheckCommandLine (void)
{
}

void I_SetGrabMouseCallback (grabmouse_callback_t func)
{
}

void I_EnableLoadingDisk(void)
{
}

void I_BindVideoVariables (void)
{
}

void I_DisplayFPSDots (boolean dots_on)
{
}

void I_CheckIsScreensaver (void)
{
}
