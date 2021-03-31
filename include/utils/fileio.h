#pragma once
#include <stdio.h>
#include <stddef.h>

typedef struct __FILEIO FILEIO;

FILEIO* fileio_open(const char* pathname, const char* mode);
int fileio_close(FILEIO* stream);
size_t fileio_read(void* ptr, size_t size, size_t nmemb, FILEIO* stream);
size_t fileio_write(void* ptr, size_t size, size_t nmemb, FILEIO* stream);
int fileio_seek(FILEIO* stream, long offset, int whence);
long fileio_tell(FILEIO* stream);
void fileio_rewind(FILEIO* stream, long offset, int whence);
int fileio_flush(FILEIO* stream);
