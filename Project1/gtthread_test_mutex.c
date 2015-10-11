#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include "gtthread.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE 
#define FALSE 0
#endif

/*
 * tests using a mutext to control access to a global variable as it
 * is incremented one by one
 */
int  count;
int  count2;
int  loops = 10;
int  preemptive = FALSE;
struct incdata
{
    gtthread_mutex_t      mp;
    int                   ctr;
    int                   ctr_num;
};

void *increment(void *pArg)
{
    struct incdata      * data = (struct incdata *) pArg;
    int                   i;
    int                   j;
    int                   k;
    int                   id = gtthread_id(gtthread_self());
    int                   rtn;
    
    for(i=0; i < loops; i++)
    {
        rtn = gtthread_mutex_lock(&data->mp);
        assert(rtn != -1);
        
        data->ctr++;
        fprintf(stdout,"In thread %d, count[%d] = %d\n", id, data->ctr_num, data->ctr);
        fflush(stdout);

        rtn = gtthread_mutex_unlock(&data->mp);
        assert(rtn != -1);

        if( ! preemptive )
        {
            gtthread_yield();
        }
        else
        {
            for(k=0; k < 1000000; k++)
            {
                j++;
            }
        }
    }
        
    return (void *) (long)id;
}


int
main(int argc, char **argv)
{
    int                       counters = 1;
    int                       cnt;
    int                       i;
    int                       j;
    int                       id;
    struct incdata          * incdatas;
    int                       opt;
    extern int                optind;
    extern char             * optarg;
    gtthread_t              * threads;
    int                       threads_per = 5;
    void                    * retval;
    int                       rtn;

    while( (opt=getopt(argc,argv, "hpn:c:t:")) != -1 )
    {
        switch(opt)
        {
            case 'p':
                preemptive = TRUE;
                break;

            case 'n':                   /* number of counters               */
                cnt = atoi(optarg);
                if( cnt < 1 )
                {
                    fprintf(stderr, "number of counters of %s too low, using %d\n",
                            optarg, counters);
                }
                else
                {
                    counters = cnt;
                }
                break;

            case 'c':                   /* number of iterations to count    */
                cnt = atoi(optarg);
                if( cnt < 1 )
                {
                    fprintf(stderr, "iterations of %s too low, using %d\n", optarg, loops);
                }
                else
                {
                    loops = cnt;
                }
                break;

            case 't':                   /* number of threads per counter    */
                cnt = atoi(optarg);
                if( cnt < 1 )
                {
                    fprintf(stderr, "threads per of %s too low, using %d\n", optarg, threads_per);
                }
                else
                {
                    threads_per = cnt;
                }
                break;

            default:
                fprintf(stderr, "Unknown options: %s\n", optarg);
                /* fall through */

            case 'h':
                fprintf(stderr, "Usage:  test_mutex [-p] [-n #] [-c #] [-t #]\n");
                fprintf(stderr, "        -p - use pre-emptive switching (vs yielding)\n");
                fprintf(stderr, "        -n # - the # of counters to manage (default: 1)\n");
                fprintf(stderr, "        -c # - the # of iterations to count (default: 10)\n");
                fprintf(stderr, "        -t # - the # of threads per counter (default: 5)\n");
                exit(10);
                break;
        }
    }    

    /*
     * initialize the threads subsystem
     */
    gtthread_init(1000);

    /*
     * allocate and construct the increment data neede for each loop
     */
    incdatas = calloc(counters, sizeof(*incdatas));
    assert(incdatas != NULL);
    for(i=0; i < counters; i++)
    {
        gtthread_mutex_init(&incdatas[i].mp);
        incdatas[i].ctr_num = i;
    }

    /*
     * allocate the thread structures and start the threads
     */
    threads = calloc(counters*threads_per, sizeof(*threads));
    assert(threads != NULL);
    for(j=0; j < threads_per; j++)
    {
        for(i=0; i < counters; i++)
        {
            rtn = gtthread_create( &threads[j*counters + i], increment, &incdatas[i]);
            assert(rtn != -1);
        }
    }

    if( ! preemptive )
    {
        gtthread_yield();
    }

    for(i=0; i < (counters*threads_per); i++)
    {
        id = gtthread_id(threads[i]);
        rtn = gtthread_join(threads[i], &retval);
        assert( rtn != -1 );
        fprintf(stdout, "Thread %d exited with the value %ld\n", id, (long) retval);
    }

    /*
     * destroy the mutexes
     */
    for(i=0; i < counters; i++)
    {
        pthread_mutex_destroy(&incdatas[i].mp);
    }
    free(incdatas);
    incdatas = NULL;
    
    free(threads);

    



    return EXIT_SUCCESS;
}
