#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <getopt.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "gtcache.h"
#include "verbosity.h"

struct testdata
{
    char        * key;
    char        * data;
    size_t        size;
};

typedef struct testdata testdata_t;

struct testdata td[] = 
{
    { "test1", "test1data", 10 },
    { "test2", "test2data", 10 },
    { "test3", "test3data", 10 },
    { "test4", "test4data", 10 },
    { "test5", "test5data", 10 },
    { "test6", "test6data", 10 },
    { "test7", "test7data", 10 },
    { "test8", "test8data", 10 },
    { "test9", "test9data", 10 },
    { "testA", "testAdata", 10 },
    { "testB", "testBdata", 10 },
    { "testC", "testCdata", 10 },
    { "testD", "testDdata", 10 },
    { "testE", "testEdata", 10 },
    { "testF", "testFdata", 10 }
};

char largebuf[1024*10];
#define SMALLBUF_CNT    5
#define SMALLBUF_LEN    (1024+1)
char smallbuf[SMALLBUF_CNT][SMALLBUF_LEN];

struct testdata td2[] = 
{
    { "test2-1", smallbuf[0], sizeof(smallbuf[0]) },
    { "test2-2", smallbuf[1], sizeof(smallbuf[1]) },
    { "test2-3", smallbuf[2], sizeof(smallbuf[2]) },
    { "test2-4", smallbuf[3], sizeof(smallbuf[3]) }
};

static int testAssert(char * test, bool condition);
static int testGet( char * test, testdata_t * pt, bool should_be_present);

bool exitOnError = false;


int main(int argc, char **argv)
{
    int           cnt = 0;
    int           i;
    int           j;
    int           opt;
    extern int    optind;
    extern char * optarg;
    testdata_t    td3;

    while( (opt=getopt(argc,argv, "hkv")) != -1 )
    {
        switch(opt)
        {
            case 'v':                   /* number of threads               */
                VERBOSE_INC();
                break;

            case 'k':
                exitOnError = true;
                break;

            default:
                fprintf(stderr, "Unknown options: %s\n", optarg);
                /* fall through */

            case 'h':
                fprintf(stderr, "Usage:  barrier_test [-n #]\n");
                fprintf(stderr, "        -h   - this help message\n");
                fprintf(stderr, "        -k   - kill test on first failure\n");
                fprintf(stderr, "        -v   - increase verbosity output(multiple ok)\n");
                exit(10);
                break;
        }
    }    

    /*
     * fill the smallbuffs
     */
    for(i=0; i < SMALLBUF_CNT; i++)
    {
        for(j=0; j < SMALLBUF_LEN; j++)
        {
            smallbuf[i][j] = '0' + j;
        }
    }

    /*
     * give us room for 3 entries
     */
    gtcache_init(1024*3, 1024, 0);

    /*
     * Test1: save/get of single item works
     */
    gtcache_set(td[0].key, td[0].data, td[0].size);
    cnt += testGet("Test  1a: save/get of single item", &td[0], true);
    cnt += testGet("Test  1b: other item not found", &td[1], false);
    
    /*
     * Test2: add a 2nd item and see it is still there
     */
    gtcache_set(td[1].key, td[1].data, td[1].size);
    cnt += testGet("Test  2a: save/get of another item", &td[1], true);
    cnt += testGet("Test  2b: can still get the 1st item", &td[0], true);
 
    /*
     * Test3: add a 3rd item and see if they are all still there
     */
    gtcache_set(td[2].key, td[2].data, td[2].size);
    cnt += testGet("Test  3a: save/get of another item", &td[2], true);
    cnt += testGet("Test  3b: can still get the 2nd item", &td[1], true);
    cnt += testGet("Test  3c: can still get the 1st item", &td[0], true);

    /*
     * Test4: add a 4th item and now 4th, 2nd and 1st should be there
     */
    gtcache_set(td[3].key, td[3].data, td[3].size);
    cnt += testGet("Test  4a: save/get of another item", &td[3], true);
    cnt += testGet("Test  4b: 3rd item is no longer in cache", &td[2], false);
    cnt += testGet("Test  4c: can still get the 2nd item", &td[1], true);
    cnt += testGet("Test  4d: can still get the 1st item", &td[0], true);

    /*
     * cleanup so we can start again
     */
    gtcache_destroy();

    /*
     * give us room for 4 entries
     */
    gtcache_init(1024*4, 1024, 0);

    /*
     * Test5: save/get of single item works
     */
    cnt += testGet("Test  5a: get w/o put gets null", &td[0], false);

    /*
     * test 6 - repeating test1 on new cache
     */
    gtcache_set(td[0].key, td[0].data, td[0].size);
    cnt += testGet("Test  6a: save/get of single item", &td[0], true);
    cnt += testGet("Test  6b: other item not found", &td[1], false);
    
    /*
     * Test7: add a 2nd item and see it is still there
     */
    gtcache_set(td[1].key, td[1].data, td[1].size);
    cnt += testGet("Test  7a: save/get of another item", &td[1], true);
    cnt += testGet("Test  7b: can still get the 1st item", &td[0], true);

    /*
     * Test8: add a 3rd item and see if they are all still there
     */
    gtcache_set(td[2].key, td[2].data, td[2].size);
    cnt += testGet("Test  8a: save/get of another item", &td[2], true);
    cnt += testGet("Test  8b: can still get the 2nd item", &td[1], true);
    cnt += testGet("Test  8c: can still get the 1st item", &td[0], true);

    /*
     * Test9: add a 4th item and all 4 should be there
     */
    gtcache_set(td[3].key, td[3].data, td[3].size);
    cnt += testGet("Test  9a: save/get of another item", &td[3], true);
    cnt += testGet("Test  9b: can still get the 3rd item", &td[2], true);
    cnt += testGet("Test  9c: can still get the 2nd item", &td[1], true);
    cnt += testGet("Test  9d: can still get the 1st item", &td[0], true);

    /*
     * Test10: add a 5th item and first 3 + 5th should be there
     */
    gtcache_set(td[4].key, td[4].data, td[4].size);
    cnt += testGet("Test 10a: save/get of another item",   &td[4], true);
    cnt += testGet("Test 10b: 4th item has been dropped",  &td[3], false);
    cnt += testGet("Test 10c: can still get the 3rd item", &td[2], true);
    cnt += testGet("Test 10d: can still get the 2nd item", &td[1], true);
    cnt += testGet("Test 10e: can still get the 1st item", &td[0], true);

    /*
     * Test11: add a 6th item and first 3 + 6th should be there
     */
    gtcache_set(td[5].key, td[5].data, td[5].size);
    cnt += testGet("Test 11a: save/get of another item",   &td[5], true);
    cnt += testGet("Test 11b: 5th item has been dropped",  &td[4], false);
    cnt += testGet("Test 11c: 4th item is still dropped",  &td[3], false);
    cnt += testGet("Test 11d: can still get the 3rd item", &td[2], true);
    cnt += testGet("Test 11e: can still get the 2nd item", &td[1], true);
    cnt += testGet("Test 11f: can still get the 1st item", &td[0], true);

    /*
     * Test12: add a 7th item and first 3 + 6th should be there
     */
    gtcache_set(td[6].key, td[6].data, td[6].size);
    cnt += testGet("Test 12a: save/get of another item",   &td[6], true);
    cnt += testGet("Test 12b: 6th item has been dropped",  &td[5], false);
    cnt += testGet("Test 12c: 5th item is still dropped",  &td[4], false);
    cnt += testGet("Test 12d: 4th item is still dropped",  &td[3], false);
    cnt += testGet("Test 12e: can still get the 3rd item", &td[2], true);
    cnt += testGet("Test 12f: can still get the 2nd item", &td[1], true);
    cnt += testGet("Test 12g: can still get the 1st item", &td[0], true);
    
    /*
     * Test13: add the 7th item again and make sure things are good
     */
    gtcache_set(td[6].key, td[6].data, td[6].size);
    cnt += testGet("Test 13a: save/get of same item again",   &td[6], true);
    cnt += testGet("Test 13b: 6th item is still dropped",  &td[5], false);
    cnt += testGet("Test 13c: 5th item is still dropped",  &td[4], false);
    cnt += testGet("Test 13d: 4th item is still dropped",  &td[3], false);
    cnt += testGet("Test 13e: can still get the 3rd item", &td[2], true);
    cnt += testGet("Test 13f: can still get the 2nd item", &td[1], true);
    cnt += testGet("Test 13g: can still get the 1st item", &td[0], true);
    
    /*
     * Test14: Adding very large item fails
     */
    gtcache_set(td[7].key, largebuf, sizeof(largebuf));
    cnt += testGet("Test 14a: can't save beyond capacity",   &td[7], false);
    cnt += testGet("Test 14b: can still get the 7th item",   &td[6], true);
    cnt += testGet("Test 14b: 6th item is still dropped",  &td[5], false);
    cnt += testGet("Test 14d: 5th item is still dropped",  &td[4], false);
    cnt += testGet("Test 14e: 4th item is still dropped",  &td[3], false);
    cnt += testGet("Test 14f: can still get the 3rd item", &td[2], true);
    cnt += testGet("Test 14g: can still get the 2nd item", &td[1], true);
    cnt += testGet("Test 14h: can still get the 1st item", &td[0], true);
    
    /*
     * cleanup so we can start again
     */
    gtcache_destroy();

    /*
     * give us room for 4 entries
     */
    gtcache_init(1024*4, 1024, 0);

    /*
     * Test15: can add 3 1K buffer but fails at 4th.
     */
    gtcache_set(td[1].key, td[1].data, td[1].size);
    cnt += testGet("Test 15a: stored 1st item", &td[1], true);
    gtcache_set(td[2].key, td[2].data, td[2].size);
    cnt += testGet("Test 15b: stored 2nd item", &td[2], true);
    gtcache_set(td[3].key, td[3].data, td[3].size);
    cnt += testGet("Test 15c: stored 3rd item", &td[3], true);
    gtcache_set(td[4].key, td[4].data, td[4].size);
    cnt += testGet("Test 15d: stored 4th item", &td[4], true);
    cnt += testGet("Test 15e: checking 4th item again", &td[4], true);
    gtcache_set(td2[0].key, td2[0].data, td2[0].size);
    cnt += testGet("Test 15f: 1st 1K buffer works",    &td2[0], true);
    cnt += testGet("Test 15g: Checking 1st 1K again",  &td2[0], true);
    cnt += testGet("Test 15h: Checking 1st 1K again",  &td2[0], true);
    gtcache_set(td2[1].key, td2[1].data, td2[1].size);
    cnt += testGet("Test 15i: 2nd 1K buffer works",    &td2[1], true);
    cnt += testGet("Test 15j: Checking 2nd 1K again",  &td2[1], true);
    cnt += testGet("Test 15k: Checking 2nd 1K again",  &td2[1], true);
    cnt += testGet("Test 15l: Checking 2nd 1K again",  &td2[1], true);
    gtcache_set(td2[2].key, td2[2].data, td2[2].size);
    cnt += testGet("Test 15m: 3rd 1K buffer works",    &td2[2], true);
    cnt += testGet("Test 15n: Checking 3rd 1K again",  &td2[2], true);
    cnt += testGet("Test 15o: Checking 3rd 1K again",  &td2[2], true);
    cnt += testGet("Test 15p: Checking 3rd 1K again",  &td2[2], true);
    gtcache_set(td2[3].key, td2[3].data, td2[3].size);
    cnt += testGet("Test 15q: 4th 1K buffer works",    &td2[3], true);
    cnt += testGet("Test 15r: 3rd 1K buffer works",    &td2[2], true);
    cnt += testGet("Test 15s: 2nd 1K buffer works",    &td2[1], true);
    cnt += testGet("Test 15t: 1st 1K buffer gone",     &td2[0], false);
   
    /*
     * Test16: add an item that takes exactly the full capacity
     */
    td3.key = "http://FullBufferTest.com";
    td3.data = largebuf;
    td3.size = 1024*4;
    gtcache_set(td3.key, td3.data, td3.size);
    cnt += testGet("Test 16a: 4K buffer fits",   &td3, true);
    cnt += testGet("Test 16b: 1st 1K buffer gone",  &td2[0], false);
    cnt += testGet("Test 16c: 2nd 1K buffer gone",  &td2[1], false);
    cnt += testGet("Test 16d: 3rd 1K buffer gone",  &td2[2], false);
    cnt += testGet("Test 16e: 4th item gone",       &td[4],  false);
    gtcache_set(td[1].key, td[1].data, td[1].size);
    cnt += testGet("Test 16f: stored 1st item", &td[1], true);
    cnt += testGet("Test 16g: 4K buffer gone",  &td3, false);
    gtcache_set(td[2].key, td[2].data, td[2].size);
    cnt += testGet("Test 16h: stored 2nd item", &td[2], true);
    gtcache_set(td[3].key, td[3].data, td[3].size);
    cnt += testGet("Test 16d: stored 3rd item", &td[3], true);
    gtcache_set(td3.key, td3.data, td3.size);
    cnt += testGet("Test 16i: 4K buffer fits again",   &td3, true);
    cnt += testGet("Test 16j: 1st item gone", &td[1], false);
    cnt += testGet("Test 16k: 2nd item gone", &td[2], false);
    cnt += testGet("Test 16l: 3rd item gone", &td[3], false);

    /*
     * Test17: Makes sure that setting an already existing entry reuses the 
     * cache memory instead of adding a completely new entry.
     */

    gtcache_destroy();
    gtcache_init(1024*2, 1024, 0);

    gtcache_set(td[0].key, td[0].data, td[0].size);
    cnt += testGet("Test 17a: stored 1st item", &td[0], true);

    char * temp_key = td[1].key;
    int temp_size = td[1].size;
    
    td[1].key = td[0].key;
    gtcache_set(td[1].key, td[1].data, td[1].size);
    cnt += testAssert("Test 17b: item added again - same size", gtcache_memused() == td[1].size);
    
    td[1].size = td[1].size * 2;
    gtcache_set(td[1].key, td[1].data, td[1].size);
    cnt += testAssert("Test 17c: item added again - larger size", gtcache_memused() == td[1].size);
    
    td[1].size = td[1].size / 2;
    gtcache_set(td[1].key, td[1].data, td[1].size);
    cnt += testAssert("Test 17d: item added again - smaller size", gtcache_memused() == td[1].size);
    
    // Restore td[1] to the original values
    td[1].key = temp_key;
    td[1].size = temp_size;

    /*
     * Test18: Invalid arguments
     */

    gtcache_destroy();
    gtcache_init(1024*2, 1024, 0);

    gtcache_set(NULL, "data", 10);
    cnt += testAssert("Test 18a: item is not stored if key is null", gtcache_memused() == 0);
    gtcache_set("key", NULL, 10);
    cnt += testAssert("Test 18b: item is not stored if data is null", gtcache_memused() == 0);
    gtcache_set("key", "data", 0);
    cnt += testAssert("Test 18c: item is not stored if size = 0", gtcache_memused() == 0);
    gtcache_set("key", "data", -1);
    cnt += testAssert("Test 18d: item is not stored if size < 0", gtcache_memused() == 0);

    
    if( cnt == 0 )
    {
        fprintf(stderr, "All tests passed\n");
    }
    else
    {
        fprintf(stderr, "FAILED:  %d tests failed\n", cnt);
    }
    return(cnt);
}

static int testAssert(char * test, bool condition) {
    int rtn = 1;

    if (condition) {
        fprintf(stderr, "%s: passed\n", test);
        rtn = 0;
    }
    else {
        fprintf(stderr, "%s: FAILED\n", test);
    }

    return rtn;
}


static int 
testGet( char * test, testdata_t * pt, bool should_be_present)
{
    void        * p = NULL;
    void        * p2 = NULL;
    int           rtn = 1;
    size_t        size;


    p = gtcache_get(pt->key, &size);

    if( ! should_be_present )
    {
        if( p != NULL )
        {
            fprintf(stderr, "%s: FAILED - %s was found\n", test, pt->key);
        }
        else
        {
            fprintf(stderr, "%s: passed\n", test);
            rtn = 0;
        }
    }
    else
    {
        p2 = gtcache_get(pt->key, NULL);

        if( p == NULL )
        {
            fprintf(stderr, "%s: FAILED - %s not found\n", test, pt->key);
        }
        else if( p2 == NULL )
        {
            fprintf(stderr, "%s: FAILED - %s not found with null size\n", test, pt->key);
        }
        else if( p == p2 )
        {
            fprintf(stderr, "%s: FAILED - not returning independent data\n", test);
        }
        else if( size != pt->size )
        {
            fprintf(stderr, "%s: FAILED - size(%ld) not %ld\n", test, size, pt->size);
        }
        else if( memcmp(p, pt->data, size) != 0 )
        {
            fprintf(stderr, "%s: FAILED - data doesn't match ('%s' vs '%s')\n",
                            test, (char *)p, pt->data);
        }
        else
        {
            fprintf(stderr, "%s: passed\n", test);
            rtn = 0;
        }
    }

    if( p != NULL )
    {
        free(p);
        p = NULL;
    }

    if( p2 != NULL )
    {
        free(p2);
        p2 = NULL;
    }

    /*
     * exit on failures if necessary
     */
    assert( (rtn == 0) || (! exitOnError) );

    return(rtn);
}
    
    
