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
#include "i_system.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef STM32F769xx
#include "stm32f769i_discovery_lcd.h"
#endif

#ifdef STM32F750xx
#include "stm32f7508_discovery_lcd.h"
#endif

static float fb_scaling = 1.0f;
int usemouse = 0;

static uint32_t dma2d_clut[256];
static byte* VideoBuffer2X = NULL;

byte *I_VideoBuffer = NULL;
boolean screensaver_mode = false;
boolean screenvisible;
float mouse_acceleration = 2.0;
int mouse_threshold = 10;
int usegamma = 4; // make it a bit brighter by default on the STM32 displays

typedef void (*scale_func_t)(const uint8_t* src, uint8_t* dst, int w, int h, float scale);
static scale_func_t selected_scaler = NULL;

DMA2D_HandleTypeDef hdma2d;

__attribute__((optimize("O3", "unroll-loops")))
static void scale_generic(const uint8_t* src, uint8_t* dst, int w, int h, float scale)
{
    int dst_w = (int)(w * scale);
    int dst_h = (int)(h * scale);

    float inv_scale = 1.0f / scale;

    for (int y = 0; y < dst_h; ++y)
    {
        int src_y = (int)(y * inv_scale);
        const uint8_t* src_row = src + src_y * w;
        uint8_t* dst_row = dst + y * dst_w;

        for (int x = 0; x < dst_w; ++x)
        {
            int src_x = (int)(x * inv_scale);
            dst_row[x] = src_row[src_x];
        }
    }
}

static void scale_2x(const uint8_t* src, uint8_t* dst, int w, int h, float scale)
{
    (void)scale;
    for (int y = 0; y < h; ++y)
    {
        const uint8_t* src_row = src + y * w;
        uint8_t* dst_row0 = dst + (y * 2) * (w * 2);
        uint8_t* dst_row1 = dst_row0 + (w * 2);
        for (int x = 0; x < w; ++x)
        {
            uint8_t p = src_row[x];
            dst_row0[x * 2 + 0] = p;
            dst_row0[x * 2 + 1] = p;
            dst_row1[x * 2 + 0] = p;
            dst_row1[x * 2 + 1] = p;
        }
    }
}

void DMA2D_Init(void)
{
    hdma2d.Instance = DMA2D;
    hdma2d.Init.Mode = DMA2D_M2M_PFC;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_ARGB8888;
    hdma2d.Init.OutputOffset = BSP_LCD_GetXSize() - (int)(fb_scaling * SCREENWIDTH);

    hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_L8;
    hdma2d.LayerCfg[1].InputOffset = 0;
    hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hdma2d.LayerCfg[1].InputAlpha = 0xFF;

    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_ConfigLayer(&hdma2d, 1);
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

void I_InitGraphics(void)
{
    int i;
    const int s_Fb_xres = BSP_LCD_GetXSize();
    const int s_Fb_yres = BSP_LCD_GetYSize();

    i = M_CheckParmWithArgs("-scaling", 1);
    if (i > 0)
    {
        fb_scaling = atof(myargv[i + 1]);
    }
    else
    {
        float sx = (float)s_Fb_xres / SCREENWIDTH;
        float sy = (float)s_Fb_yres / SCREENHEIGHT;
        fb_scaling = (sx < sy) ? sx : sy;
#ifdef STM32F769xx
        fb_scaling = 2.0f;
#endif
#ifdef STM32F750xx
        fb_scaling = 1.25f;
#endif
    }
    printf("I_InitGraphics: DOOM screen size: w x h: %d x %d\n", SCREENWIDTH, SCREENHEIGHT);
    printf("I_InitGraphics: screen size: w x h: %d x %d\n", s_Fb_xres, s_Fb_yres);
    printf("I_InitGraphics: virtual screen size: %d x %d\n",
           (int)(SCREENWIDTH*fb_scaling), (int)(SCREENHEIGHT*fb_scaling));

    if (fb_scaling == 2.0f)
    {
        selected_scaler = scale_2x;
    }
    else
    {
        selected_scaler = scale_generic;
    }

    if (!I_VideoBuffer)
    {
        I_VideoBuffer = Z_Malloc(SCREENWIDTH * SCREENHEIGHT, PU_STATIC, NULL);
    }
    screenvisible = true;
    if (fb_scaling != 1.0f && !VideoBuffer2X)
    {
        int buf_size = (int)(SCREENWIDTH * fb_scaling) * (int)(SCREENHEIGHT * fb_scaling);
        VideoBuffer2X = Z_Malloc(buf_size, PU_STATIC, NULL);
        printf("I_InitGraphics: Scale-up video buffer size %i bytes\n", buf_size);
    }

    DMA2D_Init(); // is using fb_scaling
    extern void I_InitInput(void);
    I_InitInput();
}

void I_ShutdownGraphics(void)
{
    if (I_VideoBuffer) { Z_Free(I_VideoBuffer); }
    if (VideoBuffer2X) { Z_Free(VideoBuffer2X); }
}

static void BlitDoomFrame(const uint8_t *src, uint32_t *dst, int width, int height)
{
    int out_w = (int)(fb_scaling * width);
    int out_h = (int)(fb_scaling * height);
    uint8_t* scaled = (fb_scaling == 1.0f) ? (uint8_t*)src : VideoBuffer2X;

    if (fb_scaling != 1.0f)
    {
        selected_scaler(src, scaled, width, height, fb_scaling);
    }

    uint32_t* dst_center = dst + ((BSP_LCD_GetYSize() - out_h) / 2) * BSP_LCD_GetXSize()
                                + ((BSP_LCD_GetXSize() - out_w) / 2);

    SCB_CleanDCache_by_Addr((uint32_t*)scaled, out_w * out_h);
    if (HAL_DMA2D_Start_IT(&hdma2d, (uint32_t)scaled, (uint32_t)dst_center, out_w, out_h) != HAL_OK)
    {
        I_Error("HAL_DMA2D_Start_IT failed\n");
        return;
    }
    const uint32_t timeout = HAL_GetTick() + 100; // 100ms timeout
    while (HAL_DMA2D_GetState(&hdma2d) != HAL_DMA2D_STATE_READY)
    {
        if (HAL_GetTick() > timeout)
        {
            I_Error("DMA2D timeout!\n");
            break;
        }
        __WFI(); // Let e.g. DMA2D interrupt wake us up
    }
}

void I_FinishUpdate(void)
{
    BlitDoomFrame(I_VideoBuffer, (uint32_t *)DG_ScreenBuffer, SCREENWIDTH, SCREENHEIGHT);
    DG_DrawFrame();
}

void I_StartFrame(void) {}
void I_GetEvent(void);
void I_StartTic(void) { I_GetEvent(); }
void I_UpdateNoBlit(void) {}
void I_ReadScreen(byte* scr) { memcpy(scr, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT); }

void I_SetPalette(byte* palette)
{
    for (int i = 0; i < 256; ++i)
    {
        uint8_t r = gammatable[usegamma][*palette++];
        uint8_t g = gammatable[usegamma][*palette++];
        uint8_t b = gammatable[usegamma][*palette++];
        dma2d_clut[i] = (0xFF << 24) | (r << 16) | (g << 8) | b;
    }
    DMA2D_CLUTCfgTypeDef clut_cfg =
    {
        .pCLUT = dma2d_clut,
        .CLUTColorMode = DMA2D_CCM_ARGB8888,
        .Size = 255
    };
    SCB_CleanDCache_by_Addr((uint32_t*)dma2d_clut, sizeof(dma2d_clut));
    HAL_DMA2D_CLUTLoad(&hdma2d, clut_cfg, 1);
    HAL_DMA2D_PollForTransfer(&hdma2d, HAL_MAX_DELAY);
}

int I_GetPaletteIndex(int r, int g, int b) { return 0; }
void I_BeginRead(void) {}
void I_EndRead(void) {}
void I_SetWindowTitle(char *title) { DG_SetWindowTitle(title); }
void I_GraphicsCheckCommandLine(void) {}
void I_SetGrabMouseCallback(grabmouse_callback_t func) {}
void I_EnableLoadingDisk(void) {}
void I_BindVideoVariables(void) {}
void I_DisplayFPSDots(boolean dots_on) {}
void I_CheckIsScreensaver(void) {}

