#include "fatfs_stdio.h"
#include "ff_gen_drv.h"
#include "sd_diskio.h"

#include <stdlib.h>
#include <string.h>

#define SEEK_SET 0  // Seek from beginning of file
#define SEEK_CUR 1  // Seek from current position
#define SEEK_END 2  // Seek from end of file

#define MAX_FILES       8

static FFILE g_files[MAX_FILES];
FATFS fs;
static int g_fs_initialized = 0;
static char g_fs_path[32];

int ffisopen(FIL *fp)
{
    return (fp && fp->obj.fs != NULL);
}

FFILE *ffopen(const char *path, const char *mode)
{
    BYTE fatfs_mode = 0;

    if (!g_fs_initialized)
    {
        if (FATFS_LinkDriver(&SD_Driver, g_fs_path) != 0)
        {
            return NULL;
        }
        if (f_mount(&fs, "", 1) != FR_OK)
        {
            return NULL; // Failed to mount filesystem
        }
        g_fs_initialized = 1;
    }

    if (strchr(mode, 'r')) fatfs_mode |= FA_READ;
    if (strchr(mode, 'w')) fatfs_mode |= FA_CREATE_ALWAYS | FA_WRITE;
    if (strchr(mode, 'a')) fatfs_mode |= FA_OPEN_APPEND | FA_WRITE;
    if (strchr(mode, '+')) fatfs_mode |= FA_READ | FA_WRITE;

    FFILE *f = NULL;
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (!ffisopen(&g_files[i].fil))
        {
            f = &g_files[i];
            break;
        }
    }

    if (!f) return NULL;

    if (f_open(&f->fil, path, fatfs_mode) != FR_OK) {
        return NULL;
    }

    f->error = 0;
    return f;
}

int ffclose(FFILE *f)
{
    if (!f) return -1;
    FRESULT res = f_close(&f->fil);
    return res == FR_OK ? 0 : -1;
}

size_t ffread(void *ptr, size_t size, size_t nmemb, FFILE *f)
{
    UINT br = 0;
    FRESULT res = f_read(&f->fil, ptr, size * nmemb, &br);
    if (res != FR_OK) {
        f->error = 1;
        return 0;
    }
    return br / size;
}

size_t ffwrite(const void *ptr, size_t size, size_t nmemb, FFILE *f)
{
#if _FS_READONLY
    (void)ptr; (void)size; (void)nmemb; (void)f;
    return 0;
#else
    UINT bw = 0;
    FRESULT res = f_write(&f->fil, ptr, size * nmemb, &bw);
    if (res != FR_OK) {
        f->error = 1;
        return 0;
    }
    return bw / size;
#endif
}

int ffseek(FFILE *f, long offset, int whence)
{
    FSIZE_t pos;
    switch (whence) {
        case SEEK_SET: pos = offset; break;
        case SEEK_CUR: pos = f_tell(&f->fil) + offset; break;
        case SEEK_END: pos = f_size(&f->fil) + offset; break;
        default: return -1;
    }
    return f_lseek(&f->fil, pos) == FR_OK ? 0 : -1;
}

long fftell(FFILE *f)
{
    return (long)f_tell(&f->fil);
}

int ffeof(FFILE *f)
{
    return f_tell(&f->fil) >= f_size(&f->fil);
}

int fferror(FFILE *f)
{
    return f->error;
}

int ffgetc(FFILE *f)
{
    char c;
    UINT br = 0;
    FRESULT res = f_read(&f->fil, &c, 1, &br);
    if (res != FR_OK || br != 1) {
        f->error = 1;
        return EOF;
    }
    return (unsigned char)c;
}

int ffputc(int c, FFILE *f)
{
#if _FS_READONLY
    (void)f; (void)c;
    return EOF;
#else
    UINT bw;
    char ch = (char)c;
    FRESULT res = f_write(&f->fil, &ch, 1, &bw);
    if (res != FR_OK || bw != 1) {
        f->error = 1;
        return EOF;
    }
    return ch;
#endif
}

long fflength(FFILE *f)
{
    if (!f)
        return -1;
    return f_size(&f->fil);  // uses FatFs macro
}

