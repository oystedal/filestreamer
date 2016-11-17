#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "neat.h"

#include "file_reader.h"

struct client {
    struct file_buffer **fbs;
    size_t fb_idx;
    size_t fb_count;
};

neat_error_code
on_readable(struct neat_flow_operations *ops)
{
    assert(0);
    return NEAT_OK;
}

neat_error_code
on_writable(struct neat_flow_operations *ops)
{
    unsigned int i = 0, rc = 0;
    struct client *cl = ops->userData;
    size_t len = 0;
    const unsigned char* block = (const unsigned char*)get_block(cl->fbs[cl->fb_idx], &len);

    NEAT_OPTARGS_DECLARE(1);
    NEAT_OPTARGS_INIT();

restart:
    // Check if all streams have completed
    for (; i < cl->fb_count; ++i) {
        if (!cl->fbs[i]->done)
            break;
    }

    // All streams are completed, stop
    if (i == cl->fb_count) {
        ops->on_writable = NULL;
        neat_set_operations(ops->ctx, ops->flow, ops);
        neat_shutdown(ops->ctx, ops->flow);
        return NEAT_OK;
    }

    NEAT_OPTARG_INT(NEAT_TAG_STREAM_ID, cl->fb_idx);

    neat_error_code err = neat_write(ops->ctx, ops->flow, block, len, NEAT_OPTARGS, NEAT_OPTARGS_COUNT);
    if (err == NEAT_OK) {
        rc = advance_block(cl->fbs[cl->fb_idx]);
        cl->fb_idx = (cl->fb_idx+1) % cl->fb_count;

        if (rc == 0) {
            NEAT_OPTARGS_RESET();
            goto restart;
        }
    } else if (err == NEAT_ERROR_WOULD_BLOCK) {
        cl->fb_idx = (cl->fb_idx+1) % cl->fb_count;
    } else {
        ops->on_writable = NULL;
        neat_set_operations(ops->ctx, ops->flow, ops);
        neat_close(ops->ctx, ops->flow);
    }

    return NEAT_OK;
}

neat_error_code
on_error(struct neat_flow_operations *ops)
{
    neat_close(ops->ctx, ops->flow);
    return NEAT_OK;
}

neat_error_code
on_close(struct neat_flow_operations *ops)
{
    struct client *cl = ops->userData;

    ops->on_close = NULL;
    neat_set_operations(ops->ctx, ops->flow, ops);

    for (unsigned int i = 0; i < cl->fb_count; ++i) {
        stop_reading(cl->fbs[i]);
        free(cl->fbs[i]);
    }

    free(cl->fbs);
    free(cl);
    
    neat_stop_event_loop(ops->ctx);

    return NEAT_OK;
}

neat_error_code
on_connected(struct neat_flow_operations *ops)
{
    struct client *cl;

    if ((cl = malloc(sizeof(*cl))) == NULL) {
        neat_close(ops->ctx, ops->flow);
        return NEAT_OK;
    }

    cl->fb_count = 3;
    cl->fb_idx = 0;

    if ((cl->fbs = calloc(3, sizeof(cl->fbs))) == NULL) {
        neat_close(ops->ctx, ops->flow);
        return NEAT_OK;
    }

    for (int i = 0; i < 3; ++i) {
        cl->fbs[i] = malloc(sizeof(struct file_buffer));
        start_reading(cl->fbs[i], "foo");
    }

    ops->on_writable = on_writable;
    ops->on_close = on_close;
    ops->userData = cl;
    neat_set_operations(ops->ctx, ops->flow, ops);

    return NEAT_OK;
}

int
setup_neat(void)
{
    int rc = 0;
    struct neat_ctx *ctx = NULL;
    struct neat_flow *flow = NULL;
    struct neat_flow_operations ops;
    NEAT_OPTARGS_DECLARE(1);

    NEAT_OPTARGS_INIT();
    memset(&ops, 0, sizeof(ops));

    if ((ctx = neat_init_ctx()) == NULL)
        return -1; 

    if ((flow = neat_new_flow(ctx)) == NULL) {
        rc = -1;
        goto error;
    }

    NEAT_OPTARG_INT(NEAT_TAG_STREAM_COUNT, 3);

    ops.on_readable = on_readable;
    ops.on_connected = on_connected;
    ops.on_error = on_error;
    neat_set_operations(ctx, flow, &ops);

    neat_set_property(ctx, flow, "{\"transport\": [{\"value\": \"SCTP\", \"precedence\": 2}]}");

    neat_accept(ctx, flow, 5001, NEAT_OPTARGS, NEAT_OPTARGS_COUNT);

    neat_start_event_loop(ctx, NEAT_RUN_DEFAULT);

error:
    if (ctx)
        neat_free_ctx(ctx);

    return rc;
}
