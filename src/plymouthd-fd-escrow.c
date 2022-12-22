#include <signal.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
        signal (SIGTERM, SIG_IGN);

        /* Make the first byte in argv be '@' so that we can survive systemd's killing
         * spree until the power is killed at shutdown.
         * http://www.freedesktop.org/wiki/Software/systemd/RootStorageDaemons
         */
        argv[0][0] = '@';

        while (pause());

        return 0;
}
