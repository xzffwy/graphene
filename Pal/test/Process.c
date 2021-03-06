/* -*- mode:c; c-file-style:"k&r"; c-basic-offset: 4; tab-width:4; indent-tabs-mode:nil; mode:auto-fill; fill-column:78; -*- */
/* vim: set ts=4 sw=4 et tw=78 fo=cqt wm=0: */

/* This Hello World demostrate a simple multithread program */

#define DO_BENCH    0

#include "pal.h"
#include "pal_debug.h"
#include "api.h"

int main (int argc, char ** argv)
{
    int count = 0, i;

#if DO_BEACH != 1
    pal_printf("In process: %s", argv[0]);
    for (i = 1 ; i < argc ; i++)
        pal_printf(" %s", argv[i]);
    pal_printf("\n");
#endif

    if (argc == 1) {
        unsigned long time = DkSystemTimeQuery ();
        char time_arg[24];
        pal_snprintf(time_arg, 24, "%ld", time);

        const char * newargs[4] = { "Process", "0", time_arg, NULL };

        PAL_HANDLE proc = DkProcessCreate ("file:Process", 0, newargs);

        DkObjectClose(proc);
    } else {
        count = atoi (argv[1]);

        if (count < 3)
        {
            count++;

            char count_arg[8];
            pal_snprintf(count_arg, 8, "%d", count);
            const char * newargs[4] = { "Process", count_arg, argv[2], NULL };

            PAL_HANDLE proc = DkProcessCreate ("file:Process", 0, newargs);

            DkObjectClose(proc);
        } else {
            unsigned long end = DkSystemTimeQuery ();
            unsigned long start = atol (argv[2]);
            pal_printf ("wall time = %d\n", end - start);
        }
    }

    return 0;
}

