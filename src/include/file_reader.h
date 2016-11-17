#ifndef FILE_READER_INCLUDE_H
#define FILE_READER_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

struct file_buffer {
    int fd;
    void *ptr;
    size_t len;
    size_t offset;
    int done;
};

int start_reading(struct file_buffer *filebuf, const char* filename);
const char* get_block(struct file_buffer *filebuf, size_t *len);
int advance_block(struct file_buffer *filebuf);
void unget_block(struct file_buffer *filebuf);
int stop_reading(struct file_buffer *filebuf);

#ifdef __cplusplus
}
#endif

#endif // FILE_READER_INCLUDE_H
