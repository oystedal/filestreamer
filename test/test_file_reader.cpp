#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <catch.hpp>
#include "file_reader.h"

TEST_CASE("file_reader")
{
    char testbuf[1024];

    struct file_buffer filebuf;
    struct file_buffer *fb = &filebuf;
    size_t len = 0;

    memset(fb, 0, sizeof(*fb));

    REQUIRE(start_reading(NULL, NULL) == -1);
    REQUIRE(errno == EINVAL);

    REQUIRE(start_reading(fb, "file_does_not_exist") == -1);
    REQUIRE(errno == ENOENT);

    REQUIRE(start_reading(fb, "mmap_test_input.txt") == 0);
    REQUIRE(fb->fd != 0);
    REQUIRE(fb->ptr != 0);
    REQUIRE(fb->len != 0);
    REQUIRE(fb->offset == 0);

    size_t offset = 0;
    for (int i = 1; i <= 100; ++i) {
        offset += sprintf(testbuf + offset, "%03d\n", i);
    }

    const char* block = get_block(fb, &len);
    REQUIRE(block != NULL);
    REQUIRE(len != 0);

    REQUIRE(strncmp(block, testbuf, 100*4) == 0);

    REQUIRE(advance_block(fb) == 0);

    REQUIRE(stop_reading(fb) == 0);
}
