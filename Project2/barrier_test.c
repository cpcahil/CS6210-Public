
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <sys/utsname.h>
#include <getopt.h>
#include "gtmp.h"

int main(int argc, char** argv)
{
    int                       cnt;
    int                       i;
    int                       num_iterations = 2;
    int                       num_threads = 5;
    int                       opt;
    extern int                optind;
    extern char             * optarg;

    while( (opt=getopt(argc,argv, "hn:t:")) != -1 )
    {
        switch(opt)
        {
            case 'n':                   /* number of iterations               */
                cnt = atoi(optarg);
                if( cnt < 1 )
                {
                    fprintf(stderr, "number of iterations of %s too low, using %d\n",
                            optarg, num_iterations);
                }
                else
                {
                    num_iterations = cnt;
                }
                break;

            case 't':                   /* number of threads               */
                cnt = atoi(optarg);
                if( cnt < 1 )
                {
                    fprintf(stderr, "number of threads of %s too low, using %d\n",
                            optarg, num_threads);
                }
                else
                {
                    num_threads = cnt;
                }
                break;

            default:
                fprintf(stderr, "Unknown options: %s\n", optarg);
                /* fall through */

            case 'h':
                fprintf(stderr, "Usage:  barrier_test [-n #]\n");
                fprintf(stderr, "        -h   - this help message\n");
                fprintf(stderr, "        -n # - the # of iterations to run (default: 2)\n");
                fprintf(stderr, "        -t # - the # of threads to use (default: 5)\n");
                exit(10);
                break;
        }
    }    



    /*
     * Prevents runtime from adjusting the number of threads.
     */
    omp_set_dynamic(0);

    /*
     * Making sure that it worked.
     */
    if (omp_get_dynamic())
    {
        printf("Warning: dynamic adjustment of threads has been set\n");
    }

    /*
     * Setting number of threads
     */
    omp_set_num_threads(num_threads);

    /*
     * initializing barrier for number of threads
     */
    gtmp_init(num_threads);

    /*
     * loop through the number of barriers we're supposed to hit
     */
    for(i=0; i < num_iterations; i++ )
    {
        /*
         * parallel block
         */
        #pragma omp parallel 
        {

            fprintf(stdout, "before barrier %d...\n", i);
            fflush(stdout);

            /*
             * The barrier, instead of #pragma barrier
             */
            gtmp_barrier();

            fprintf(stdout, "after barrier %d...\n", i);
            fflush(stdout);

            /*
             * The barrier, instead of #pragma barrier
             */
            gtmp_barrier();
        }

    }

    gtmp_finalize();

    return(0);
}
