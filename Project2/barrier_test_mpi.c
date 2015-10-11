#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <getopt.h>
#include <assert.h>
#include "mpi.h"
#include "gtmpi.h"


int main(int argc, char **argv)
{
    int           cnt;
    int           i;
    int           my_id;
    int           num_iterations = 2;
    int           num_processes;
    int           num_threads = 5;
    int           opt;
    extern int    optind;
    extern char * optarg;

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

    gtmpi_init(num_threads);

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &num_processes);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_id);

    /*
     * make sure we're configured with the right number of threads & processes
     */
    assert( num_threads == num_processes );

    for(i=0; i < num_iterations; i++)
    {

        fprintf(stdout, "thread[%d]: before barrier %d...\n", my_id, i);
        fflush(stdout);

        /*
         * The barrier
         */
        gtmpi_barrier();

        fprintf(stdout, "thread[%d]: after barrier %d...\n", my_id, i);
        fflush(stdout);

        /*
         * The barrier
         */
        gtmpi_barrier();

    }

    MPI_Finalize();
    gtmpi_finalize();

    return 0;
}

