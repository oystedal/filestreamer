#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/sctp.h>

#include <netdb.h>

#include "file_reader.h"

static int listen_fd = 0;

struct client {
    int fd;
    struct file_buffer **fbs;
    size_t fb_idx;
    size_t fb_count;
};

int
setup_listen_socket(void)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family    = AF_UNSPEC;
    hints.ai_socktype  = SOCK_STREAM;
    hints.ai_flags     = AI_PASSIVE;
    hints.ai_protocol  = IPPROTO_SCTP;
    hints.ai_canonname = NULL;
    hints.ai_addr      = NULL;
    hints.ai_next      = NULL;

    s = getaddrinfo(NULL, "5001", &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;                  /* Success */

        close(sfd);
    }

    if (rp == NULL) {
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    freeaddrinfo(result);

    struct sctp_initmsg initmsg;
    memset(&initmsg, 0, sizeof(initmsg));

    initmsg.sinit_num_ostreams = 32;

    if (setsockopt(sfd, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg)) < 0)
        return -1;

    if (listen(sfd, SOMAXCONN) < 0) {
        return -1;
    }

    listen_fd = sfd;
    return 0;
}

int
new_client(int efd, int listen_socket)
{
    int rc = 0;
    int new_fd = 0;
    struct epoll_event eev;
    struct client *new_client = NULL;

    if ((new_client = malloc(sizeof(*new_client))) == NULL) {
        errno = ENOMEM;
        rc = -1;
        goto error;
    }

    memset(new_client, 0, sizeof(*new_client));

    if ((new_client->fbs = calloc(3, sizeof(new_client->fbs))) == NULL) {
        errno = ENOMEM;
        rc = -1;
        goto error;
    }

    if ((new_fd = accept(listen_socket, NULL, NULL)) < 0) {
        perror("accept");
        goto error;
    }

    new_client->fd = new_fd;
    new_client->fb_count = 3;
    new_client->fb_idx = 0;

    eev.events = EPOLLOUT;
    eev.data.ptr = new_client;

    if (epoll_ctl(efd, EPOLL_CTL_ADD, new_fd, &eev) < 0) {
        perror("epoll_ctl");
        goto error;
    }

    fcntl(new_fd, F_SETFL, O_NONBLOCK);

    for (int i = 0; i < 3; ++i) {
        new_client->fbs[i] = malloc(sizeof(struct file_buffer));
        start_reading(new_client->fbs[i], "foo");
    }

    return rc;
error:
    if (new_fd) close(new_fd);
    if (new_client) free(new_client);

    return -1;
}


void
close_client(int efd, struct client *cl)
{
    printf("close_client\n");
    if (epoll_ctl(efd, EPOLL_CTL_DEL, cl->fd, NULL) < 0)
        perror("epoll_ctl");
    if (close(cl->fd) < 0)
        perror("close");

    for (unsigned int i = 0; i < cl->fb_count; ++i) {
        stop_reading(cl->fbs[i]);
        free(cl->fbs[i]);
    }

    free(cl);
}

int 
read_data(struct client *cl)
{
    char buf[1025];

    int rc = recv(cl->fd, (void*)buf, 1024, 0);
    if (rc < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return 0;

        perror("recv");

        return -1;
    } else if (rc == 0) {
        return -1;
    }

    buf[rc+1] = '\0';

    fprintf(stdout, "%s\n", buf);

    return 0;
}

int
write_data(struct client *cl)
{
    struct msghdr msghdr;
    struct iovec iov;
    char cmsgbuf[CMSG_SPACE(sizeof(struct sctp_sndrcvinfo))];

    memset(&msghdr, 0, sizeof(msghdr));
    memset(cmsgbuf, 0, sizeof(cmsgbuf));

    struct cmsghdr *cmsg;
    cmsg = (struct cmsghdr *)cmsgbuf;
    cmsg->cmsg_level = IPPROTO_SCTP;
    cmsg->cmsg_type = SCTP_SNDRCV;
    cmsg->cmsg_len = CMSG_LEN(sizeof(struct sctp_sndrcvinfo));

    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_control = cmsgbuf;
    msghdr.msg_controllen = CMSG_SPACE(sizeof(struct sctp_sndrcvinfo));
    msghdr.msg_iov = &iov;
    msghdr.msg_iovlen = 1;

    struct sctp_sndrcvinfo *sndrcvinfo;
    sndrcvinfo = (struct sctp_sndrcvinfo *)CMSG_DATA(cmsg);

    unsigned int i = 0;

restart:
    // Check if all streams have completed
    for (; i < cl->fb_count; ++i) {
        if (!cl->fbs[i]->done)
            break;
    }

    // All streams are completed, stop
    if (i == cl->fb_count)
        return 1;

    for (;;) {
        sndrcvinfo->sinfo_stream = cl->fb_idx;

        size_t len = 0;
        const char* block = get_block(cl->fbs[cl->fb_idx], &len);

        iov.iov_base = (void*)block;
        iov.iov_len = len;

        int rc = sendmsg(cl->fd, &msghdr, 0);
        if (rc < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                cl->fb_idx = (cl->fb_idx+1) % cl->fb_count;
                break;
            }

            perror("sendmsg");
            return -1;
        }

        rc = advance_block(cl->fbs[cl->fb_idx]);
        cl->fb_idx = (cl->fb_idx+1) % cl->fb_count;

        if (rc == 0)
            goto restart;
    }

    return 0;
}

int
do_poll(void)
{
    int efd;
    struct epoll_event eev;
    struct epoll_event events[32];

    if ((efd = epoll_create(32)) < 0) {
        perror("epoll_create");
        return -1;
    }

    struct client listencl;
    listencl.fd = listen_fd;

    eev.events = EPOLLIN;
    eev.data.ptr = &listencl;

    if (epoll_ctl(efd, EPOLL_CTL_ADD, listen_fd, &eev)) {
        perror("epoll_ctl");
        return -1;
    }

    int event_count;
    while ((event_count = epoll_wait(efd, events, sizeof(events) / sizeof(*events), -1)) >= 0) {
        fprintf(stdout, "%d epoll events\n", event_count);
        for (int i = 0; i < event_count; i++) {
            struct epoll_event *event = &events[i];
            struct client *cl = (struct client*)event->data.ptr;

            if (cl->fd == listen_fd) {
                if (new_client(efd, listen_fd) < 0)
                    goto oom;
                continue;
            }

            if (event->events & (EPOLLHUP | EPOLLERR)) {
                close_client(efd, cl);
                continue;
            }

            if (event->events & EPOLLOUT) {
                int rc = write_data(cl);

                if (rc == -1) {
                    close_client(efd, cl);
                } else if (rc == 1) {
                    printf("Shutting down client...\n");
                    if (shutdown(cl->fd, SHUT_RDWR) < 0) {
                        perror("shutdown");
                        close_client(efd, cl);
                    }

                    event->events = EPOLLIN;

                    if (epoll_ctl(efd, EPOLL_CTL_MOD, cl->fd, event) < 0) {
                        perror("epoll_ctl");
                        close_client(efd, cl);
                    }
                }
            }

            if (event->events & EPOLLIN) {
                if (read_data(cl) < 0)
                    close_client(efd, cl);
            }

        }
    }

    perror("epoll_wait");
    return -1;
oom:
    fprintf(stderr, "Out of memory");
    return -1;
}
