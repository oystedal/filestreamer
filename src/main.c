#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/stat.h>

#ifdef WITH_NEAT
#include "server_neat.h"
#else
#include "server_sockets.h"
#endif

void
usage(char *argv[])
{
    printf("Usage:\n");
    printf("\t%s [-p 5001] <file 1> [file 2 ... file N]\n", argv[0]);
}

int
main(int argc, char *argv[])
{
    const char* port = "5001";
    for (;;) {
        int rc = getopt(argc, argv, "p:");

        if (rc == -1) {
            break;
        } else if (rc == 'p') {
            port = optarg;
        } else {
            usage(argv);
            return EXIT_SUCCESS;
        }
    }

    if (argc - optind == 0) {
        usage(argv);
        return EXIT_SUCCESS;
    }

    for (int i = optind; i < argc; ++i) {
        int rc;
        struct stat sbuf;
        if ((rc = stat(argv[i], &sbuf)) != 0) {
            printf("Could not open %s: %s\n", argv[i], strerror(errno));
            return EXIT_FAILURE;
        }
    }

#ifdef WITH_NEAT
    setup_neat(port, argc-optind, argv+optind);
#else
    setup_listen_socket(port, argc-optind, argv+optind);
    do_poll();
#endif

    return EXIT_SUCCESS;
}

