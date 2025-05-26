#ifndef FATFS_STDIO_H
#define FATFS_STDIO_H

#ifdef STM32F769xx

#include "ff.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    FIL fil;
    int error;
} FFILE;

FFILE *ffopen(const char *path, const char *mode);
int    ffclose(FFILE *f);
size_t ffread(void *ptr, size_t size, size_t nmemb, FFILE *f);
size_t ffwrite(const void *ptr, size_t size, size_t nmemb, FFILE *f);
int    ffseek(FFILE *f, long offset, int whence);
long   fftell(FFILE *f);
int    ffeof(FFILE *f);
int    fferror(FFILE *f);
int    ffgetc(FFILE *f);
int    ffputc(int c, FFILE *f);
long   fflength(FFILE *f);

#ifdef __cplusplus
}
#endif

#endif // STM32F769xx

#endif // FATFS_STDIO_H

