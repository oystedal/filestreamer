#include "server_sockets.h"

#ifdef WITH_NEAT
int setup_neat(void);
#endif

int main(int argc, char *argv[])
{
#ifdef WITH_NEAT
    setup_neat();
#else
    setup_listen_socket();
    do_poll();
#endif

    return 0;
}

