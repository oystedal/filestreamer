#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "file_reader.h"

#define BLOCK_SIZE 4096

int
start_reading(struct file_buffer *filebuf, const char* filename)
{
    struct stat sbuf;
    void* tmp;

    if (filebuf == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (stat(filename, &sbuf) < 0)
        return -1;

    if ((filebuf->fd = open(filename, O_RDONLY)) < 0)
        return -1;

    if ((tmp = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, filebuf->fd, 0)) == MAP_FAILED)
        return -1;

    filebuf->done = 0;
    filebuf->ptr = tmp;
    filebuf->len = sbuf.st_size;
    filebuf->offset = 0;

    return 0;
}

const char*
get_block(struct file_buffer *filebuf, size_t *len)
{
    const char *tmp = (char*)filebuf->ptr + filebuf->offset;
    *len = filebuf->len - filebuf->offset;
    if (*len > BLOCK_SIZE) {
        *len = BLOCK_SIZE;
    }
    return tmp;
}

int
advance_block(struct file_buffer *filebuf)
{
    filebuf->offset += BLOCK_SIZE;
    if (filebuf->offset > filebuf->len) {
        filebuf->done = 1;
        return 0;
    }
    return 1;
}

void
unget_block(struct file_buffer *filebuf)
{
}

int
stop_reading(struct file_buffer *filebuf) {
    if (munmap(filebuf->ptr, filebuf->len) < 0) 
        return -1;

    if (close(filebuf->fd) < 0)
        return -1;

    return 0;
}
