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

char largebuf[1024*100];
#define SMALLBUF_CNT    5
#define SMALLBUF_LEN    1024
char smallbuf[SMALLBUF_CNT][SMALLBUF_LEN];

testdata_t        td2[] = 
{
    { "test2-1", smallbuf[0], sizeof(smallbuf[0])-1 },
    { "test2-2", smallbuf[1], sizeof(smallbuf[1])-1 },
    { "test2-3", smallbuf[2], sizeof(smallbuf[2])-1 },
    { "test2-4", smallbuf[3], sizeof(smallbuf[3])-1 }
};

testdata_t        td3[10];      // will be set inline later
testdata_t        td4[] = 
{
    { "test1", largebuf,  2047 },
    { "test2", largebuf, 16383 },
    { "test3", largebuf,  8191 },
    { "test4", largebuf,  4095 },
    { "test5", largebuf,  2047 },
    { "test6", largebuf, 16383 },
    { "test7", largebuf,  8191 },
    { "test8", largebuf,  4095 }
};

testdata_t      td5[] =
{
    { "test1", largebuf, 2047 },
    { "test2", largebuf, 2047 },
    { "test3", largebuf, 2047 },
    { "test4", largebuf, 2047 },
    { "test5", largebuf, 2047 },
    { "test6", largebuf, 4095 },
    { "test7", largebuf, 4095 },
    { "test8", largebuf, 4095 },
    { "test9", largebuf, 4095 },
    { "testA", largebuf, 4095 }
};

testdata_t      td6[] =
{
    { "test1", largebuf, 16383 },
    { "test2", largebuf,  4095 },
    { "test3", largebuf,  8191 },
    { "test4", largebuf, 16383 },
    { "test5", largebuf,  4095 },
    { "test6", largebuf,  8191 },
    { "test7", largebuf,  2047 },
    { "test8", largebuf,  2047 },
    { "test9", largebuf,  2047 }
};

static int testGet( char * test, testdata_t * pt, bool should_be_present);
static void TestSet(testdata_t * pt);

bool exitOnError = false;


int main(int argc, char **argv)
{
    int           cnt = 0;
    int           i;
    int           j;
    int           opt;
    extern int    optind;
    extern char * optarg;
    int           size;

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
    gtcache_init(1024*3, 1024, 2);

    /*
     * Test1: save/get of single item works
     */
    TestSet(td+0);
    cnt += testGet("Test  1a: save/get of single item", &td[0], true);
    cnt += testGet("Test  1b: other item not found", &td[1], false);
    
    /*
     * Test2: add a 2nd item and see it is still there
     */
    TestSet(td+1);
    cnt += testGet("Test  2a: save/get of another item", &td[1], true);
    cnt += testGet("Test  2b: can still get the 1st item", &td[0], true);
 
    /*
     * Test3: add a 3rd item and see if they are all still there
     */
    TestSet(td+2);
    cnt += testGet("Test  3a: save/get of another item", &td[2], true);
    cnt += testGet("Test  3b: can still get the 2nd item", &td[1], true);
    cnt += testGet("Test  3c: can still get the 1st item", &td[0], true);

    /*
     * Test4: add a 4th item and now 4th, 2nd and 1st should be there
     */
    TestSet(td+3);
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
    gtcache_init(1024*4, 1024, 3);

    /*
     * Test5: save/get of single item works
     */
    cnt += testGet("Test  5a: get w/o put gets null", &td[0], false);

    /*
     * test 6 - repeating test1 on new cache
     */
    TestSet(td+0);
    cnt += testGet("Test  6a: save/get of single item", &td[0], true);
    cnt += testGet("Test  6b: other item not found", &td[1], false);
    
    /*
     * Test7: add a 2nd item and see it is still there
     */
    TestSet(td+1);
    cnt += testGet("Test  7a: save/get of another item", &td[1], true);
    cnt += testGet("Test  7b: can still get the 1st item", &td[0], true);

    /*
     * Test8: add a 3rd item and see if they are all still there
     */
    TestSet(td+2);
    cnt += testGet("Test  8a: save/get of another item", &td[2], true);
    cnt += testGet("Test  8b: can still get the 2nd item", &td[1], true);
    cnt += testGet("Test  8c: can still get the 1st item", &td[0], true);

    /*
     * Test9: add a 4th item and all 4 should be there
     */
    TestSet(td+3);
    cnt += testGet("Test  9a: save/get of another item", &td[3], true);
    cnt += testGet("Test  9b: can still get the 3rd item", &td[2], true);
    cnt += testGet("Test  9c: can still get the 2nd item", &td[1], true);
    cnt += testGet("Test  9d: can still get the 1st item", &td[0], true);

    /*
     * Test10: add a 5th item and first 3 + 5th should be there
     */
    TestSet(td+4);
    cnt += testGet("Test 10a: save/get of another item",   &td[4], true);
    cnt += testGet("Test 10b: 4th item has been dropped",  &td[3], false);
    cnt += testGet("Test 10c: can still get the 3rd item", &td[2], true);
    cnt += testGet("Test 10d: can still get the 2nd item", &td[1], true);
    cnt += testGet("Test 10e: can still get the 1st item", &td[0], true);

    /*
     * Test11: add a 6th item and first 3 + 6th should be there
     */
    TestSet(td+5);
    cnt += testGet("Test 11a: save/get of another item",   &td[5], true);
    cnt += testGet("Test 11b: 5th item has been dropped",  &td[4], false);
    cnt += testGet("Test 11c: 4th item is still dropped",  &td[3], false);
    cnt += testGet("Test 11d: can still get the 3rd item", &td[2], true);
    cnt += testGet("Test 11e: can still get the 2nd item", &td[1], true);
    cnt += testGet("Test 11f: can still get the 1st item", &td[0], true);

    /*
     * Test12: add a 7th item and first 3 + 6th should be there
     */
    TestSet(td+6);
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
    TestSet(td+6);
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
    td3[0].key = "http://FullBufferTest.com";
    td3[0].data = largebuf;
    td3[0].size = sizeof(largebuf);
    TestSet(&td3[0]);
    cnt += testGet("Test 14a: can't save beyond capacity",   &td3[0], false);
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
    gtcache_init(1024*4, 1024, 5);

    /*
     * Test15: can add 3 1K buffer but fails at 4th.
     */
    gtcache_destroy();
    gtcache_init(1024*4 - 1, 1024, 3);
    TestSet(td+1);
    cnt += testGet("Test 15a: stored 1st item", &td[1], true);
    TestSet(td+2);
    cnt += testGet("Test 15b: stored 2nd item", &td[2], true);
    TestSet(td+3);
    cnt += testGet("Test 15c: stored 3rd item", &td[3], true);
    TestSet(td+4);
    cnt += testGet("Test 15d: stored 4th item", &td[4], true);
    cnt += testGet("Test 15e: checking 4th item again", &td[4], true);
    TestSet(td2+0);
    gtcache_set(td2[0].key, td2[0].data, td2[0].size);
    cnt += testGet("Test 15f: 1st 1K buffer works",    &td2[0], true);
    cnt += testGet("Test 15g: Checking 1st 1K again",  &td2[0], true);
    cnt += testGet("Test 15h: Checking 1st 1K again",  &td2[0], true);
    TestSet(td2+1);
    cnt += testGet("Test 15i: 2nd 1K buffer works",    &td2[1], true);
    cnt += testGet("Test 15j: Checking 2nd 1K again",  &td2[1], true);
    cnt += testGet("Test 15k: Checking 2nd 1K again",  &td2[1], true);
    cnt += testGet("Test 15l: Checking 2nd 1K again",  &td2[1], true);
    TestSet(td2+2);
    cnt += testGet("Test 15m: 3rd 1K buffer works",    &td2[2], true);
    cnt += testGet("Test 15n: Checking 3rd 1K again",  &td2[2], true);
    cnt += testGet("Test 15o: Checking 3rd 1K again",  &td2[2], true);
    cnt += testGet("Test 15p: Checking 3rd 1K again",  &td2[2], true);
    TestSet(td2+3);
    cnt += testGet("Test 15q: 4th 1K buffer works",    &td2[3], true);
    cnt += testGet("Test 15r: 3rd 1K buffer works",    &td2[2], true);
    cnt += testGet("Test 15s: 2nd 1K buffer works",    &td2[1], true);
    cnt += testGet("Test 15t: 1st 1K buffer gone",     &td2[0], false);
   
    /*
     * Test16: add an item that takes exactly the full capacity
     */
    td3[0].key = "http://FullBufferTest.com";
    td3[0].data = largebuf;
    td3[0].size = 1024*4-1;
    TestSet(&td3[0]);
    cnt += testGet("Test 16a: 4K buffer fits",   &td3[0], true);
    cnt += testGet("Test 16b: 1st 1K buffer gone",  &td2[0], false);
    cnt += testGet("Test 16c: 2nd 1K buffer gone",  &td2[1], false);
    cnt += testGet("Test 16d: 3rd 1K buffer gone",  &td2[2], false);
    cnt += testGet("Test 16e: 4th item gone",       &td[4],  false);
    TestSet(td+1);
    cnt += testGet("Test 16f: stored 1st item", &td[1], true);
    cnt += testGet("Test 16g: 4K buffer gone",  &td3[0], false);
    TestSet(td+2);
    cnt += testGet("Test 16h: stored 2nd item", &td[2], true);
    TestSet(td+3);
    cnt += testGet("Test 16d: stored 3rd item", &td[3], true);
    TestSet(&td3[0]);
    cnt += testGet("Test 16i: 4K buffer fits again",   &td3[0], true);
    cnt += testGet("Test 16j: 1st item gone", &td[1], false);
    cnt += testGet("Test 16k: 2nd item gone", &td[2], false);
    cnt += testGet("Test 16l: 3rd item gone", &td[3], false);

    /*
     * Test17: explicit test for lru - reset cache to 2K total,
     * place 2 1k blocks into cache to fill it.  Access 1st block 3
     * times.  Access 2nd block.  add a 3rd block -- should replace
     * 1st block even though it has been accessed more frequently
     */
    gtcache_destroy();
    gtcache_init(1024*2, 1024, 3);
    TestSet(&td3[0]);
    cnt += testGet("Test 17a: 4k key wont fit in 2k cache", &td3[0], false);
    TestSet(td2+1);
    TestSet(td2+2);
    cnt += testGet("Test 17b: access 1st item 1st time", &td2[1], true);
    cnt += testGet("Test 17c: access 1st item 2nd time", &td2[1], true);
    cnt += testGet("Test 17d: access 1st item 3rd time", &td2[1], true);
    cnt += testGet("Test 17e: access 2nd item 1st time", &td2[2], true);
    TestSet(td2+3);
    cnt += testGet("Test 17f: access 3rd item 1st time", &td2[3], true);
    cnt += testGet("Test 17g: 1st item is gone", &td2[1], false);
    cnt += testGet("Test 17e: 2nd item still here", &td2[2], true);
   
    /*
     * test18 - explicit test for lrumin  
     *        - set a bunch of small entries.  Add a 1K entry. verify they exist.
     *          add a 2nd 1k entry. verify it replaces the first 1k entry;
     */
    gtcache_destroy();
    gtcache_init(1024*2, 500, 2);
    td3[0].key = "http://FullBufferTest.com";
    td3[0].data = largebuf;
    td3[0].size = 999;
    TestSet(&td3[0]);
    TestSet(td2+0);
    cnt += testGet("Test 18a: 1k key set works", &td2[0], true);
    TestSet(td2+1);
    cnt += testGet("Test 18b: 2nd 1k key works", &td2[1], true);
    cnt += testGet("Test 18c: 1st 1k key gone",  &td2[0], false);
    cnt += testGet("Test 18d: original key is still present", &td3[0], true);
    
    /*
     * test19 - same test as 18, but with larger level so both 1K blocks should stay
     *        - set a small entries.  Add a 1K entry. verify they exist.
     *          add a 2nd 1k entry. verify it replaces small blocks;
     */
    gtcache_destroy();
    gtcache_init(1024*2, 512, 2);
    TestSet(&td3[0]);
    TestSet(td2+0);
    cnt += testGet("Test 19a: 1k key set works", &td2[0], true);
    TestSet(td2+1);
    cnt += testGet("Test 19b: 2nd 1k key works", &td2[1], true);
    cnt += testGet("Test 19c: 1st 1k key still present",  &td2[0], true);
    cnt += testGet("Test 19d: original key is replaced", &td3[0], false);
   
    /*
     * test20 - test multiple levels and replacing particular entries.
     *        - create 61K or so cache fill it to the brink.
     *          add additional 8K entry, should cause 1st 16k entry to be dropped
     *          NOTE: this is not how I expected it to work.  But it is how the 
     *          udacity testing is verifying it
     */ 
    gtcache_destroy();
    for(i=0, size=0; i < sizeof(td4)/sizeof(*td4); i++)
    {
        size += td4[i].size;
    }
    gtcache_init(size, 1024, 4);

    for(i=0; i < sizeof(td4)/sizeof(*td4); i++)
    {
        TestSet(&td4[i]);
    }
    td3[0].key = "http://Overflow8KBuffer.com";
    td3[0].data = largebuf;
    td3[0].size = 8191;
    TestSet(&td3[0]);

    cnt += testGet("Test 20a: 1st key is still present", &td4[0], true);
    cnt += testGet("Test 20b: 2nd key is gone",          &td4[1], false);
    cnt += testGet("Test 20c: 3rd key is still present", &td4[2], true);
    cnt += testGet("Test 20d: 4th key is still present", &td4[3], true);
    cnt += testGet("Test 20e: 5th key is still present", &td4[4], true);
    cnt += testGet("Test 20f: 6th key is still present", &td4[5], true);
    cnt += testGet("Test 20g: 7th key is still present", &td4[6], true);
    cnt += testGet("Test 20h: 8th key is still present", &td4[7], true);
    cnt += testGet("Test 20i: 9th key is there",         &td3[0], true);

    
    /*
     * test21 - now replace another 8K element, should just add the element
     */ 
    td3[1].key = "http://8KBuffer.com";
    td3[1].data = largebuf;
    td3[1].size = 8191;
    TestSet(&td3[1]);
    cnt += testGet("Test 21a: 1st key is still present", &td4[0], true);
    cnt += testGet("Test 21b: 2nd key is gone",          &td4[1], false);
    cnt += testGet("Test 21c: 3rd key is still present", &td4[2], true);
    cnt += testGet("Test 21d: 4th key is still present", &td4[3], true);
    cnt += testGet("Test 21e: 5th key is still present", &td4[4], true);
    cnt += testGet("Test 21f: 6th key is still present", &td4[5], true);
    cnt += testGet("Test 21g: 7th key is still present", &td4[6], true);
    cnt += testGet("Test 21h: 8th key is still present", &td4[7], true);
    cnt += testGet("Test 21i: 9th key is still present", &td3[0], true);
    cnt += testGet("Test 21j: new key is there",         &td3[1], true);
    
    /*
     * test22 - now replace a 16k element, should replace the 2nd 16K entry
     */ 
    td3[2].key = "http://16Kbuffer.com";
    td3[2].data = largebuf;
    td3[2].size = 16383;
    TestSet(&td3[2]);
    cnt += testGet("Test 22a: 1st key is still present", &td4[0], true);
    cnt += testGet("Test 22b: 2nd key is gone",          &td4[1], false);
    cnt += testGet("Test 22c: 3rd key is still present", &td4[2], true);
    cnt += testGet("Test 22d: 4th key is still present", &td4[3], true);
    cnt += testGet("Test 22e: 5th key is still present", &td4[4], true);
    cnt += testGet("Test 22f: 6th key is gone",          &td4[5], false);
    cnt += testGet("Test 22g: 7th key is still present", &td4[6], true);
    cnt += testGet("Test 22h: 8th key is still present", &td4[7], true);
    cnt += testGet("Test 22i: 9th key is still present", &td3[0], true);
    cnt += testGet("Test 22j: 10th key is there",        &td3[1], true);
    cnt += testGet("Test 22k: new key is there",         &td3[2], true);

    
    /*
     * test23 - now replace a 2k element, should replace the 4th item (a 4K item)
     */ 
    td3[3].key = "http://2Kbuffer.com";
    td3[3].data = largebuf;
    td3[3].size = 2047;
    TestSet(&td3[3]);
    cnt += testGet("Test 23a: 1st key is still present", &td4[0], true);
    cnt += testGet("Test 23b: 2nd key is gone",          &td4[1], false);
    cnt += testGet("Test 23c: 3rd key is gone",          &td4[2], false);
    cnt += testGet("Test 23d: 4th key is still present", &td4[3], true);
    cnt += testGet("Test 23e: 5th key is still present", &td4[4], true);
    cnt += testGet("Test 23f: 6th key is gone",          &td4[5], false);
    cnt += testGet("Test 23g: 7th key is still present", &td4[6], true);
    cnt += testGet("Test 23h: 8th key is still present", &td4[7], true);
    cnt += testGet("Test 23i: 9th key is still present", &td3[0], true);
    cnt += testGet("Test 23j: 10th key is there",        &td3[1], true);
    cnt += testGet("Test 23k: new key is there",         &td3[2], true);
    cnt += testGet("Test 23l: new key is there",         &td3[3], true);

    /*
     * Test24 - test recursive replacement 
     *          create an array of elements which fit into the 2nd and
     *          3rd levels.  try to add an element that fits into the 
     *          4th level but is of a size that an object from the 3rd
     *          and then 2nd level needs to be dropped
     *          so, 2K and 4K elements added, then add 5K element which 
     *          will require one 4K and one 2K element to be dropped.
     *          verify correct entries exist afterwards.
     */
    gtcache_destroy();
    
    for(i=0, size=0; i < sizeof(td5)/sizeof(*td5); i++)
    {
        size += td5[i].size;
    }
    gtcache_init(size, 512, 4);

    for(i=0; i < sizeof(td5)/sizeof(*td5); i++)
    {
        TestSet(&td5[i]);
    }

    td3[0].key = "http://5Kbuffer.com";
    td3[0].data = largebuf;
    td3[0].size = 1024*5-1;
    TestSet(&td3[0]);

    cnt += testGet("Test 24a: 1st 2k key is gone",          &td5[0], false);
    cnt += testGet("Test 24b: 2nd 2k key is there",         &td5[1], true);
    cnt += testGet("Test 24c: 3rd 2k key is there",         &td5[2], true);
    cnt += testGet("Test 24d: 4th 2k key is there",         &td5[3], true);
    cnt += testGet("Test 24e: 5th 2k key is there",         &td5[4], true);
    cnt += testGet("Test 24f: 1st 4k key is gone",          &td5[5], false);
    cnt += testGet("Test 24g: 2nd 4k key is there",         &td5[6], true);
    cnt += testGet("Test 24h: 3rd 4k key is there",         &td5[7], true);
    cnt += testGet("Test 24i: 4th 4k key is there",         &td5[8], true);
    cnt += testGet("Test 24j: 5th 4k key is there",         &td5[9], true);
    cnt += testGet("Test 24k: new key is there",            &td3[0], true);
            
    /*
     * Test25 - test eviction of LRU element across multiple parent bands
     *          add a bunch of items of 2, 4, 8, and 16K sizes to fill buffer
     *          with the 16K buffer being first (oldest).  Try to add a 1K 
     *          buffer afterwards to see if the oldest one gets deleted
     */
    gtcache_destroy();
    
    for(i=0, size=0; i < sizeof(td6)/sizeof(*td6); i++)
    {
        size += td6[i].size;
    }
    gtcache_init(size, 512, 5);

    for(i=0; i < sizeof(td6)/sizeof(*td6); i++)
    {
        TestSet(&td6[i]);
    }

    td3[0].key = "http://1Kbuffer.com";
    td3[0].data = largebuf;
    td3[0].size = 1024-1;
    TestSet(&td3[0]);

    cnt += testGet("Test 25a: 1st key is gone",          &td6[0], false);
    cnt += testGet("Test 25b: 2nd key is there",         &td6[1], true);
    cnt += testGet("Test 25c: 3rd key is there",         &td6[2], true);
    cnt += testGet("Test 25d: 4th key is there",         &td6[3], true);
    cnt += testGet("Test 25e: 5th key is there",         &td6[4], true);
    cnt += testGet("Test 25f: 6th key is there",         &td6[5], true);
    cnt += testGet("Test 25g: 7th key is there",         &td6[6], true);
    cnt += testGet("Test 25h: 8th key is there",         &td6[7], true);
    cnt += testGet("Test 25i: 9th key is there",         &td6[8], true);
    cnt += testGet("Test 25j: new key is there",         &td3[0], true);
            
    /*
     * Test26 - test eviction of LRU element across multiple parent bands
     *          same as test25 but with a 4K buffer
     */
    gtcache_destroy();
    
    for(i=0, size=0; i < sizeof(td6)/sizeof(*td6); i++)
    {
        size += td6[i].size;
    }
    gtcache_init(size, 512, 5);

    for(i=0; i < sizeof(td6)/sizeof(*td6); i++)
    {
        TestSet(&td6[i]);
    }

    td3[0].key = "http://4Kbuffer.com";
    td3[0].data = largebuf;
    td3[0].size = 1024*4-1;
    TestSet(&td3[0]);

    cnt += testGet("Test 26a: 1st key is gone",          &td6[0], false);
    cnt += testGet("Test 26b: 2nd key is there",         &td6[1], true);
    cnt += testGet("Test 26c: 3rd key is there",         &td6[2], true);
    cnt += testGet("Test 26d: 4th key is there",         &td6[3], true);
    cnt += testGet("Test 26e: 5th key is there",         &td6[4], true);
    cnt += testGet("Test 26f: 6th key is there",         &td6[5], true);
    cnt += testGet("Test 26g: 7th key is there",         &td6[6], true);
    cnt += testGet("Test 26h: 8th key is there",         &td6[7], true);
    cnt += testGet("Test 26i: 9th key is there",         &td6[8], true);
    cnt += testGet("Test 26j: new key is there",         &td3[0], true);


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

static void
TestSet(testdata_t * pt)
{
    gtcache_set(pt->key, pt->data, pt->size);
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
    
    
