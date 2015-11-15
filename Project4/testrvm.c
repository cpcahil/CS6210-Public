#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <getopt.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "rvm.h"
#include "verbosity.h"

bool exitOnError = false;

#define TEST_DIR    "testdir_cpc"
#define TEST_SEG    "cpc_test_seg_name"
#define STR_FAILED  "===FAILED==="
#define STR_PASSED  "passed"
#define MAX_SEGPATH_SIZE    512

struct testseg
{
    char    * segName;
    int       segSize;
    int       success;
    char    * data;
};
typedef struct testseg testseg_t;

rvm_t       rvm;

/*
 * definitions/structures for Test_rvm_map()
 */
#define PARMS_TM_SCRIPT    0
#define PARMS_TM_TESTSEG   1
#define PARMS_TM_CNT       2
#define PARMS_TM_SIZE      3
#define TEST_BLOCK_SIZE     8192

static testseg_t testmult_diff[] =
{
    { TEST_SEG "-multi-1", 4096*1, 1, NULL },
    { TEST_SEG "-multi-2", 4096*2, 1, NULL },
    { TEST_SEG "-multi-3", 4096*3, 1, NULL },
    { TEST_SEG "-multi-4", 4096*4, 1, NULL },
    { TEST_SEG "-multi-5", 4096*5, 1, NULL },
    { TEST_SEG "-multi-6", 4096*6, 1, NULL }
};

static testseg_t testmult_samenamesize[] =
{
    { TEST_SEG "-samenamesize-1", 4096*1, 1, NULL },
    { TEST_SEG "-samenamesize-1", 4096*1, 0, NULL }
};

static testseg_t testmult_samename[] =
{
    { TEST_SEG "-samename-1", 4096*1, 1, NULL },
    { TEST_SEG "-samename-1", 4096*2, 0, NULL }
};

static void FillInData(char * pName, char * pData, size_t cnt);
static int  RunInChild( int (*func)(int cnt, void **parms), int cnt, void **parms);
static int  Test_rvm_init();
static int  Test_rvm_map();
static int  Test_rvm_unmap();
static int  Test_rvm_destroy();
static int  Test_rvm_begin_trans();
static int  Test_rvm_about_to_modify();
static int  TestModify( int parmcnt, void **parms );
static int  TestMultiInit(int cnt, void **parms);
static int  TestBeginTrans( int parmcnt, void **parms );
static int  TestDestroy( int parmcnt, void **parms );
static int  TestGoodInit(int cnt, void **parms);
static int  TestMapCheckSuccess(void * pMem, testseg_t * pTest, bool silent);
static int  TestMapMultiple(int cnt, void **parms);
static int  TestUnmap( int cnt, void **parms );
static int  TestSuccess(bool success );

int main(int argc, char **argv)
{
    int           cnt = 0;
    int           opt;
    extern int    optind;
    extern char * optarg;

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
                fprintf(stderr, "Usage:  testrvm [-h] [-v] [-k]\n");
                fprintf(stderr, "        -h   - this help message\n");
                fprintf(stderr, "        -k   - kill test on first failure\n");
                fprintf(stderr, "        -v   - increase verbosity output(multiple ok)\n");
                exit(10);
                break;
        }
    }

    /*
     * Test1: test rvm_init...
     */
    cnt += Test_rvm_init();

    /*
     * Test2: test rvm_map...
     */
    cnt += Test_rvm_map();

    /*
     * Test3: test rvm_unmap()...
     */
    cnt += Test_rvm_unmap();

    /*
     * Test4: test rvm_destroy()...
     */
    cnt += Test_rvm_destroy();

    /*
     * Test5: test rvm_begin_trans()...
     */
    cnt += Test_rvm_begin_trans();

    /*
     * Test6: test rvm_about_to_modify()...
     */
    cnt += Test_rvm_about_to_modify();

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

#define LONGNAME  \
        "ThisIsA140CharacterDirectoryNameThatExceedsTheSizeOfPrefixDefinedInrvm" \
        "_tSoThisShouldNotBeAllowedToBeCreatedByTheFunctionrvminitWellSeeIfItIs"

static int
Test_rvm_init()
{
    int           cnt = 0;
    char        * failed = STR_FAILED "\n";
    char        * passed = STR_PASSED "\n";

    fprintf(stdout, "------------ Test 1: unit tests for rvm_int() ------------\n");

    /*
     * Test A: Try initializing with NULL, should fail
     */
    fprintf(stdout, "Test 1a: Initialize with a NULL directory: ");
    if( (rvm = rvm_init(NULL)) != NULL )
    {
        fputs(failed,stdout);
        if( exitOnError )
        {
            exit(10);
        }
        cnt++;
    }
    else
    {
        fputs(passed, stdout);
    }

    /*
     * Test B: Try initializing with a directory name that is too long, should fail
     */
    fprintf(stdout, "Test 1b: Initialize with a too long dir name: ");
    if( (rvm = rvm_init(LONGNAME)) != NULL )
    {
        fputs(failed,stdout);
        if( exitOnError )
        {
            exit(10);
        }
        cnt++;
    }
    else
    {
        fputs(passed, stdout);
    }

    /*
     * Test C: try initializing over a file with the same name
     */
    system("rm -rf " TEST_DIR "; touch " TEST_DIR);
    fprintf(stdout, "Test 1c: Initialize dir name that already exists as a file: ");
    cnt += TestSuccess( (rvm = rvm_init(TEST_DIR)) == NULL );

    /*
     * Test D: try initializing over a directory who's parent doesn't exist
     */
    unlink(TEST_DIR);
    fprintf(stdout, "Test 1d: Missing parent directory fails: ");
    cnt += TestSuccess( (rvm = rvm_init(TEST_DIR "/dir2")) == NULL );

    /*
     * Test E: try initializing over a directory without permissions to create files
     */
    system("rm -rf " TEST_DIR "; mkdir " TEST_DIR "; chmod 444 " TEST_DIR );
    fprintf(stdout, "Test 1e: Existing directory with too little permissions fails: ");
    cnt += TestSuccess( (rvm = rvm_init(TEST_DIR)) == NULL );
    system("rm -rf " TEST_DIR );

    /*
     * Test F: Basic Initialization with no existing director succeeds
     */
    system("rm -rf " TEST_DIR );
    fprintf(stdout, "Test 1f: Basic init with no existing directory succeeds: ");
    fflush(stdout);
    cnt += RunInChild(TestGoodInit, 0, NULL);

    /*
     * Test G: Initialization with existing initalized directory succeeds (just run with previous init)
     */
    fprintf(stdout, "Test 1g: Basic init with existing, initialized directory succeeds: ");
    fflush(stdout);
    cnt += RunInChild(TestGoodInit, 0, NULL);

    /*
     * Test H: Basic Initialization with existing, uninitialized  directory succeeds
     */
    system("rm -rf " TEST_DIR "; mkdir " TEST_DIR );
    fprintf(stdout, "Test 1h: Basic init with existing empty directory succeeds: ");
    fflush(stdout);
    cnt += RunInChild(TestGoodInit, 0, NULL);

    /*
     * Test I: Try double initialization (do it in a child so we don't mess up our testing)
     */
    system("rm -rf " TEST_DIR " " TEST_DIR "2");
    fprintf(stdout, "Test 1i: Double initialization fails: ");
    fflush(stdout);
    cnt += RunInChild(TestMultiInit, 0, NULL);


    return(cnt);
}

static int
TestGoodInit(int cnt, void ** parms)
{
    rvm_t         rvm1;

    /*
     * this needs to be tested in a child process
     */
    return( TestSuccess( (rvm1 = rvm_init(TEST_DIR)) != NULL ) );
}

static int
TestMultiInit(int cnt, void **parms)
{
    rvm_t         rvm1;
    rvm_t         rvm2;
    int           rtn = 0;

    /*
     * this needs to be tested in a child process
     */
    if( (rvm1 = rvm_init(TEST_DIR)) == NULL )
    {
        fprintf( stdout, STR_FAILED " - initial init failed\n");
        if( exitOnError )
        {
            exit(10);
        }
        rtn++;
    }
    else if( (rvm2 = rvm_init(TEST_DIR "2")) != NULL )
    {
        fprintf( stdout, STR_FAILED " - 2nd init succeeded\n");
        if( exitOnError )
        {
            exit(10);
        }
        rtn++;
    }
    else
    {
        fprintf(stdout, STR_PASSED "\n");
    }
    return(rtn);
}

static int
Test_rvm_map()
{
    int               cnt = 0;
    void            * parms[PARMS_TM_SIZE];
    char            * pFile;
    struct stat       statbuf;
    testseg_t         test;
    char              testdata[TEST_BLOCK_SIZE];
    struct _rvm_t     testrvm;

    fprintf(stdout, "------------ Test 2: unit tests for rvm_map() ------------\n");

    /*
     * Test A: Try initializing with NULL, should fail
     */
    system("rm -rf " TEST_DIR " " TEST_DIR "2");
    fprintf(stdout, "Test 2a: call with a NULL rvm fails: ");
    cnt += TestSuccess( (rvm = rvm_map(NULL, "testseg1", 10)) == NULL );

    /*
     * Test B: witn an uninitialized rvm
     */
    memset(&testrvm, '\0', sizeof(testrvm));
    fprintf(stdout, "Test 2b: call with an uninitialized rvm fails: ");
    cnt += TestSuccess( (rvm = rvm_map(&testrvm, "testseg1", 10)) == NULL );

    /*
     * Test C: with a NULl segment name fails
     */
    system("rm -rf " TEST_DIR);
    fprintf(stdout, "Test 2c: call with a NULL segment name fails: ");
    fflush(stdout);
    test.segName = NULL;
    test.segSize = 4096;
    test.success = false;
    test.data    = NULL;
    parms[PARMS_TM_SCRIPT]  = NULL;                // no pre-map script to run (after rvm init)
    parms[PARMS_TM_TESTSEG] = (void *) &test;
    parms[PARMS_TM_CNT]     = (void *) 1;
    cnt += RunInChild(TestMapMultiple, sizeof(parms)/sizeof(parms[0]), parms);

    /*
     * Test D: with a to long segment name fails
     */
    system("rm -rf " TEST_DIR);
    fprintf(stdout, "Test 2d: call with a too long segment name fails: ");
    fflush(stdout);
    test.segName = LONGNAME;
    test.segSize = 4096;
    test.success = false;
    test.data    = NULL;
    parms[PARMS_TM_SCRIPT]  = NULL;                // no pre-map script to run (after rvm init)
    parms[PARMS_TM_TESTSEG] = (void *) &test;
    parms[PARMS_TM_CNT]     = (void *) 1;
    cnt += RunInChild(TestMapMultiple, sizeof(parms)/sizeof(parms[0]), parms);

    /*
     * Test E: with a normal segment name and size succeeds
     */
    system("rm -rf " TEST_DIR);
    fprintf(stdout, "Test 2e: call to create a normal segment succeeds: ");
    fflush(stdout);
    test.segName = TEST_SEG;
    test.segSize = 4096;
    test.success = true;
    test.data    = NULL;
    parms[PARMS_TM_SCRIPT]  = NULL;                // no pre-map script to run (after rvm init)
    parms[PARMS_TM_TESTSEG] = (void *) &test;
    parms[PARMS_TM_CNT]     = (void *) 1;
    cnt += RunInChild(TestMapMultiple, sizeof(parms)/sizeof(parms[0]), parms);

    /*
     * setup test data for subsequent tests
     */
    memset(testdata, ' ', sizeof(testdata));
    memcpy(testdata, "Hello", 5);
    memcpy(&testdata[2048], "World", 5);
    memcpy(&testdata[4096], "Just", 4);
    memcpy(&testdata[8000], "Kidding", 7);

    /*
     * Test F: mapping with existing data succeeds
     */
    FillInData(TEST_DIR "/" TEST_SEG, testdata, 4096);      // place data in the file
    fprintf(stdout, "Test 2f: call with existing data in segment succeeds: ");
    fflush(stdout);
    test.segName = TEST_SEG;
    test.segSize = 4096;
    test.success = true;
    test.data    = testdata;
    parms[PARMS_TM_SCRIPT]  = NULL;                // no pre-map script to run (after rvm init)
    parms[PARMS_TM_TESTSEG] = (void *) &test;
    parms[PARMS_TM_CNT]     = (void *) 1;
    cnt += RunInChild(TestMapMultiple, sizeof(parms)/sizeof(parms[0]), parms);

    /*
     * Test G: mapping with less data succeeds
     */
    fprintf(stdout, "Test 2g: call with less data than segment succeeds: ");
    fflush(stdout);
    test.segName = TEST_SEG;
    test.segSize = 2048;
    test.success = true;
    test.data    = testdata;
    parms[PARMS_TM_SCRIPT]  = NULL;                // no pre-map script to run (after rvm init)
    parms[PARMS_TM_TESTSEG] = (void *) &test;
    parms[PARMS_TM_CNT]     = (void *) 1;
    cnt += RunInChild(TestMapMultiple, sizeof(parms)/sizeof(parms[0]), parms);

    /*
     * Test H: verify file has not changed size
     */
    fprintf(stdout, "Test 2h: file size hasn't changed: ");
    pFile = TEST_DIR "/" TEST_SEG;
    if( stat(pFile, &statbuf) == -1 )
    {
        fprintf(stdout, STR_FAILED " - segment '%s' not found\n", pFile);
        if( exitOnError )
        {
            exit(10);
        }
        cnt++;
    }
    else if( statbuf.st_size != 4096 )
    {
        fprintf(stdout, STR_FAILED " - wrong size (%ld, expecting 4096\n", (long) statbuf.st_size);
        if( exitOnError )
        {
            exit(10);
        }
        cnt++;
    }
    else
    {
        fprintf(stdout, STR_PASSED "\n");
    }

    /*
     * Test I: mapping with more data on existing segment
     */
    fprintf(stdout, "Test 2i: call with increased segment size succeeds: ");
    fflush(stdout);
    test.segName = TEST_SEG;
    test.segSize = 8192;
    test.success = true;
    test.data    = NULL;
    parms[PARMS_TM_SCRIPT]  = NULL;                // no pre-map script to run (after rvm init)
    parms[PARMS_TM_TESTSEG] = (void *) &test;
    parms[PARMS_TM_CNT]     = (void *) 1;
    cnt += RunInChild(TestMapMultiple, sizeof(parms)/sizeof(parms[0]), parms);

    /*
     * Test J: verify file has increased
     */
    fprintf(stdout, "Test 2j: verify file size has increased: ");
    pFile = TEST_DIR "/" TEST_SEG;
    if( stat(pFile, &statbuf) == -1 )
    {
        fprintf(stdout, STR_FAILED " - segment '%s' not found\n", pFile);
        if( exitOnError )
        {
            exit(10);
        }
        cnt++;
    }
    else if( statbuf.st_size != 8192 )
    {
        fprintf(stdout, STR_FAILED " - wrong size (%ld, expecting 8192\n", (long) statbuf.st_size);
        if( exitOnError )
        {
            exit(10);
        }
        cnt++;
    }
    else
    {
        fprintf(stdout, STR_PASSED "\n");
    }

    /*
     * Test K: mapping the first 4 k gets me the original data
     */
    fprintf(stdout, "Test 2k: mapping 1st 4k gets original data succeeds: ");
    fflush(stdout);
    test.segName = TEST_SEG;
    test.segSize = 4096;
    test.success = true;
    test.data    = testdata;
    parms[PARMS_TM_SCRIPT]  = NULL;                // no pre-map script to run (after rvm init)
    parms[PARMS_TM_TESTSEG] = (void *) &test;
    parms[PARMS_TM_CNT]     = (void *) 1;
    cnt += RunInChild(TestMapMultiple, sizeof(parms)/sizeof(parms[0]), parms);

    /*
     * Test L: mapping the full 8k gets me the original 4k + 4k nulls
     */
    memset(testdata+4096, '\0', sizeof(testdata)-4096);
    fprintf(stdout, "Test 2l: mapping full seg gets 4K data + 4k nulls succeeds: ");
    fflush(stdout);
    test.segName = TEST_SEG;
    test.segSize = 8192;
    test.success = true;
    test.data    = testdata;
    parms[PARMS_TM_SCRIPT]  = NULL;                // no pre-map script to run (after rvm init)
    parms[PARMS_TM_TESTSEG] = (void *) &test;
    parms[PARMS_TM_CNT]     = (void *) 1;
    cnt += RunInChild(TestMapMultiple, sizeof(parms)/sizeof(parms[0]), parms);

    /*
     * Test M: mapping multiple segments succeeds
     */
    fprintf(stdout, "Test 2m: mapping multiple segments succeeds: ");
    fflush(stdout);
    parms[PARMS_TM_SCRIPT]  = NULL;                // no pre-map script to run (after rvm init)
    parms[PARMS_TM_TESTSEG] = (void *) testmult_diff;
    parms[PARMS_TM_CNT]     = (void *) (sizeof(testmult_diff)/sizeof(testmult_diff[0]));
    cnt += RunInChild(TestMapMultiple, sizeof(parms)/sizeof(parms[0]), parms);

    /*
     * Test N: mapping same segment with same size fails
     */
    fprintf(stdout, "Test 2n: mapping same segments with same size fails: ");
    fflush(stdout);
    parms[PARMS_TM_SCRIPT]  = NULL;                // no pre-map script to run (after rvm init)
    parms[PARMS_TM_TESTSEG] = (void *) testmult_samenamesize;
    parms[PARMS_TM_CNT]     = (void *) (sizeof(testmult_samenamesize)/sizeof(testmult_samenamesize[0]));
    cnt += RunInChild(TestMapMultiple, sizeof(parms)/sizeof(parms[0]), parms);

    /*
     * Test O: mapping same segment with diff size fails
     */
    fprintf(stdout, "Test 2o: mapping same segments with diff size also fails: ");
    fflush(stdout);
    parms[PARMS_TM_SCRIPT]  = NULL;                // no pre-map script to run (after rvm init)
    parms[PARMS_TM_TESTSEG] = (void *) testmult_samename;
    parms[PARMS_TM_CNT]     = (void *) (sizeof(testmult_samename)/sizeof(testmult_samename[0]));
    cnt += RunInChild(TestMapMultiple, sizeof(parms)/sizeof(parms[0]), parms);


    /*
     * Other tests:
     *
     *  mapping with logs to apply
     *  mapping with existing log file but no logs to apply
     */


    return(cnt);

} /* Test_rvm_map(... */

static int
TestMapMultiple(int cnt, void **parms)
{
    int               i;
    testseg_t       * maps;
    void            **pMem;
    int               rtn = 0;
    rvm_t             rvm1;
    bool              silent = false;

    assert(cnt == PARMS_TM_SIZE);

    /*
     * this needs to be tested in a child process
     */
    if( (rvm1 = rvm_init(TEST_DIR)) == NULL )
    {
        fprintf( stdout, STR_FAILED " - can't create RVM\n");
        if( exitOnError )
        {
            exit(10);
        }
        rtn++;
    }
    else
    {

        maps = (testseg_t *) parms[PARMS_TM_TESTSEG];
        cnt  = (int) (long) parms[PARMS_TM_CNT];

        if( cnt > 1 )
        {
            silent = true;
        }

        assert( (pMem = calloc(cnt, sizeof(void*))) != NULL );

        for(i=0; i < cnt; i++)
        {
            /*
             * map the specified segment
             */
            pMem[i] = rvm_map(rvm1, maps[i].segName, maps[i].segSize);

            rtn += TestMapCheckSuccess(pMem[i], &maps[i], silent);
        }

        /*
         * now go unmap any necessary modules
         */
        for(i=0; i < cnt; i++)
        {
            if(pMem[i] != NULL)
            {
                rvm_unmap(rvm1, pMem);
                pMem[i] = NULL;
            }
        }

        if( silent )
        {
            if( rtn == 0 )
            {
                fprintf(stdout, STR_PASSED "\n");
            }
            else
            {
                fprintf(stdout, STR_FAILED "\n");
            }
        }
    }

    return(rtn);

} /* TestMapMultiple(... */

static int
TestMapCheckSuccess(void * pMem, testseg_t * pTest, bool silent)
{
    char              filePath[MAX_SEGPATH_SIZE];
    int               rtn = 0;
    struct stat       statbuf;

    /*
     * if this behaved as we expected
     */
    if( pTest->success && (pMem != NULL) )
    {
        /*
         * check that the object exists and is the right size
         */
        sprintf(filePath, TEST_DIR "/%s", pTest->segName);
        if( stat(filePath, &statbuf) == -1 )
        {
            if( ! silent || exitOnError )
            {
                fprintf(stdout, STR_FAILED " - segment '%s' not found\n", filePath);
                if( exitOnError )
                {
                    exit(10);
                }
            }
            rtn++;
        }
        else if( statbuf.st_size < pTest->segSize )
        {
            if( ! silent || exitOnError )
            {
                fprintf(stdout, STR_FAILED " - segment '%s' incorrect size (%ld, expecting %ld)\n",
                                            filePath, (long) statbuf.st_size, (long) pTest->segSize );
                if( exitOnError )
                {
                    exit(10);
                }
            }
            rtn++;
        }
        /*
         * else if there's supposed to be data there and it doesn't match
         */
        else if( (pTest->data != NULL) && (memcmp(pTest->data, pMem, pTest->segSize) != 0 ) )
        {
            if( ! silent || exitOnError )
            {
                fprintf(stdout, STR_FAILED " - data doesn't match expected\n");
                if( exitOnError )
                {
                    exit(10);
                }
            }
            rtn++;
        }
        else if ( ! silent )
        {
            fprintf(stdout, STR_PASSED "\n");
        }
    }
    else if( (! pTest->success ) && (pMem == NULL) )
    {
        if( ! silent )
        {
            fprintf(stdout, STR_PASSED "\n");
        }
    }
    else if( pTest->success )
    {
        if( ! silent || exitOnError )
        {
            fprintf( stdout, STR_FAILED " - NULL returned from rvm_map\n");
            if( exitOnError )
            {
                exit(10);
            }
        }
        rtn++;
    }
    else
    {
        if( ! silent || exitOnError )
        {
            fprintf( stdout, STR_FAILED " - 0x%lx returned from rvm_map\n", (long) pMem);
            if( exitOnError )
            {
                exit(10);
            }
        }
        rtn++;
    }

    return(rtn);

} /* TestMapCheckSuccess(... */

static int
Test_rvm_unmap()
{
    int               cnt = 0;

    fprintf(stdout, "------------ Test 3: unit tests for rvm_unmap() ------------\n");

    /*
     * just run all of these tests in a single child
     */
    system("rm -rf " TEST_DIR);
    cnt += RunInChild(TestUnmap, 0, NULL);


    return(cnt);
}

static int
TestUnmap( int parmcnt, void **parms )
{
    int               cnt = 0;
    int               errcnt;
    char            * failed = STR_FAILED "\n";
    int               i;
    testseg_t       * maps;
    int               max;
    char            * passed = STR_PASSED "\n";
    char            * pMem;
    char            * pMem2;
    void            **pMems1;
    void            **pMems2;
    rvm_t             rvm1;
    struct _rvm_t     testrvm;
    char              testval;

    /*
     * unlike our other tests, rvm_unmap() doesn't inicate success or failure so we can only
     * check by attempting to access the data... should prove interesting if things aren't 
     * working correctly.
     */

    /*
     * get our handle, if we can't, no tests can be run...
     */
    if( (rvm1 = rvm_init(TEST_DIR)) == NULL )
    {
        fprintf( stdout, "Test 3 initialization - " STR_FAILED " - can't create RVM\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }

    /*
     * map a simple segment  (again for testing);
     */
    if( (pMem = rvm_map(rvm1, TEST_SEG, 4096)) == NULL )
    {
        fprintf( stdout, "Test 3 initialization - " STR_FAILED " - can't map 1st seg\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }
    testval = pMem[0];
    
    /*
     * Test A:  unmapping with NULL rvm
     */
    fprintf(stdout, "Test 3a: call with a NULL rvm fails: ");
    rvm_unmap(NULL,  pMem);

    /*
     * this will crash if the memory has been unmapped
     */
    if( pMem[0] == testval )
    {
        fputs(passed, stdout);
    }
    else
    {
        fputs(failed,stdout);
        if( exitOnError )
        {
            exit(10);
        }
        cnt++;
    }
        
    /*
     * Test B: unmap witn an uninitialized rvm
     */
    memset(&testrvm, '\0', sizeof(testrvm));
    fprintf(stdout, "Test 3b: call with an uninitialized rvm fails: ");
    rvm_unmap(&testrvm,  pMem);

    /*
     * this will crash if the memory has been unmapped
     */
    if( pMem[0] == testval )
    {
        fputs(passed, stdout);
    }
    else
    {
        fputs(failed,stdout);
        if( exitOnError )
        {
            exit(10);
        }
        cnt++;
    }
        
    /*
     * Test C: with a NULl segment name fails
     */
    fprintf(stdout, "Test 3c: call with a NULL segment name fails: ");
    rvm_unmap(rvm1,  NULL);
    
    if( pMem[0] == testval )
    {
        fputs(passed, stdout);
    }
    else
    {
        fputs(failed,stdout);
        if( exitOnError )
        {
            exit(10);
        }
        cnt++;
    }

    /*
     * Test D: unmap of non-base pointer fails
     */
    fprintf(stdout, "Test 3d: call with off by 1024 bytes pointer fails: ");
    rvm_unmap(rvm1,  pMem+1024);
    if( (pMem2 = rvm_map(rvm1, TEST_SEG, 4096)) != NULL )
    {
        fputs(failed,stdout);
        if( exitOnError )
        {
            exit(10);
        }
        cnt++;
    }
    else
    {
        fputs(passed, stdout);
    }

    /*
     * Test E: off by 1 pointer fails
     */
    fprintf(stdout, "Test 3e: call with off by 1 byte (+) pointer fails: ");
    rvm_unmap(rvm1,  pMem+1);
    if( (pMem2 = rvm_map(rvm1, TEST_SEG, 4096)) != NULL )
    {
        fputs(failed,stdout);
        if( exitOnError )
        {
            exit(10);
        }
        cnt++;
    }
    else
    {
        fputs(passed, stdout);
    }

    /*
     * Test F: off by 1 byte (-) pointer fails
     */
    fprintf(stdout, "Test 3f: call with off by 1 byte (-) pointer fails: ");
    rvm_unmap(rvm1,  pMem-1);
    if( (pMem2 = rvm_map(rvm1, TEST_SEG, 4096)) != NULL )
    {
        fputs(failed,stdout);
        if( exitOnError )
        {
            exit(10);
        }
        cnt++;
    }
    else
    {
        fputs(passed, stdout);
    }


    /*
     * Test G: verify we can map again after unmap (Note: this one is correctly replacing pMem)
     */
    fprintf(stdout, "Test 3g: map again after unmap succeeds: ");
    rvm_unmap(rvm1,  pMem);
    if( (pMem = rvm_map(rvm1, TEST_SEG, 4096)) == NULL )
    {
        fputs(failed,stdout);
        if( exitOnError )
        {
            exit(10);
        }
        cnt++;
    }
    else
    {
        fputs(passed, stdout);
    }

    /*
     * test H: map multiple, they cannot be remaped, then unmap, then remap
     */
    fprintf(stdout, "Test 3h: map/unmap/map multiple succeeds: ");
    max = sizeof(testmult_diff)/sizeof(testmult_diff[0]);
    maps = testmult_diff;
    assert( (pMems1 = calloc(max, sizeof(void*))) != NULL );
    assert( (pMems2 = calloc(max, sizeof(void*))) != NULL );

    errcnt = 0;
    for(i=0; i < max; i++)
    {
        pMems1[i] = rvm_map(rvm1, maps[i].segName, maps[i].segSize);
        errcnt += TestMapCheckSuccess(pMems1[i], &maps[i], true);
    }

    for(i=0; i < max; i++)
    {
        rvm_unmap(rvm1, pMems1[i]);
    }

    for(i=0; i < max; i++)
    {
        pMems1[i] = rvm_map(rvm1, maps[i].segName, maps[i].segSize);
        errcnt += TestMapCheckSuccess(pMems1[i], &maps[i], true);
    }

    if( errcnt != 0 )
    {
        fputs(failed,stdout);
        if( exitOnError )
        {
            exit(10);
        }
        cnt++;
    }
    else
    {
        fputs(passed, stdout);
    }
        
    /*
     * test I: trying to remap those same segments again all fail
     */
    fprintf(stdout, "Test 3i: remap multiple segs all fail: ");
    max = sizeof(testmult_diff)/sizeof(testmult_diff[0]);
    maps = testmult_diff;
    errcnt = 0;
    for(i=0; i < max; i++)
    {
        /*
         * change success state to failure for this test -- we don't expect any of 
         * these segments to be mappable since they are already mapped
         */
        maps[i].success = false;
        pMems2[i] = rvm_map(rvm1, maps[i].segName, maps[i].segSize);
        errcnt += TestMapCheckSuccess(pMems2[i], &maps[i], true);
        /*
         * undo our change
         */
        maps[i].success = true;
    }
    if( errcnt != 0 )
    {
        fputs(failed,stdout);
        if( exitOnError )
        {
            exit(10);
        }
        cnt++;
    }
    else
    {
        fputs(passed, stdout);
    }

    /*
     * clean up after those two tests
     */
    for(i=0; i < max; i++)
    {
        rvm_unmap(rvm1, pMems1[i]);
        pMems1[i] = NULL;
    }
    free(pMems1);
    free(pMems2);

    pMems1 = pMems2 = NULL;

    /*
     * additional tests
     *      unmapping a segment that has a transaction in progress
     */

    return(cnt);

} /* TestUnmap(... */

static int
Test_rvm_destroy()
{
    int               cnt = 0;

    fprintf(stdout, "------------ Test 4: unit tests for rvm_destroy() ------------\n");

    /*
     * just run all of these tests in a single child
     */
    system("rm -rf " TEST_DIR);
    cnt += RunInChild(TestDestroy, 0, NULL);


    return(cnt);
}

static int
TestDestroy( int parmcnt, void **parms )
{
    int               cnt = 0;
    int               errcnt;
    char              filePath[512];
    int               i;
    testseg_t       * maps;
    int               max;
    char            * pFileName;
    char            * pMem;
    char            * pMem2;
    void            **pMems1;
    rvm_t             rvm1;
    struct stat       statbuf;
    struct _rvm_t     testrvm;

    /*
     * get our handle, if we can't, no tests can be run...
     */
    if( (rvm1 = rvm_init(TEST_DIR)) == NULL )
    {
        fprintf( stdout, "Test 4 initialization - " STR_FAILED " - can't create RVM\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }

    /*
     * map a simple segment  (again for testing);
     */
    if( (pMem = rvm_map(rvm1, TEST_SEG, 4096)) == NULL )
    {
        fprintf( stdout, "Test 4 initialization - " STR_FAILED " - can't map 1st seg\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }

    /*
     * map a 2nd simple segment  (again for testing);
     */
    if( (pMem2 = rvm_map(rvm1, TEST_SEG "2", 4096)) == NULL )
    {
        fprintf( stdout, "Test 4 initialization - " STR_FAILED " - can't map 2nd seg\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }
    rvm_unmap(rvm1, pMem2);  // TEST_SEG "2" is now a segment we can destroy
    pFileName = TEST_DIR "/" TEST_SEG "2";
    

    /*
     * Test A:  destroying with NULL rvm
     */
    fprintf(stdout, "Test 4a: call with a NULL rvm fails: ");
    rvm_destroy(NULL,  TEST_SEG "2");

    /*
     * check to make sure the file still exists
     */
    cnt += TestSuccess( stat(pFileName, &statbuf) == 0 );
        
    /*
     * Test B: destroying witn an uninitialized rvm
     */
    memset(&testrvm, '\0', sizeof(testrvm));
    fprintf(stdout, "Test 4b: call with an uninitialized rvm fails: ");
    rvm_destroy(&testrvm,  TEST_SEG "2");

    /*
     * check to make sure the file still exists
     */
    cnt += TestSuccess( stat(pFileName, &statbuf) == 0 );

    /*
     * Test C: with a NULl segment name fails
     */
    fprintf(stdout, "Test 4c: call with a NULL segment name fails: ");
    rvm_destroy(rvm1,  NULL);
    
    /*
     * this is a lame check... but I had to add something..  the real 
     * test is that we didn't crash above
     */
    cnt += TestSuccess( stat(pFileName, &statbuf) == 0 );

    /*
     * Test D: destroying a real segment works
     */
    fprintf(stdout, "Test 4d: destroying an unused segment succeeds: ");
    rvm_destroy(rvm1,  TEST_SEG "2");
    
    /*
     * segment should not be there
     */
    cnt += TestSuccess( stat(pFileName, &statbuf) == -1 );

    /*
     * Test E: destroying a real segment works
     */
    fprintf(stdout, "Test 4e: destroying a destroyed segment doesn't crash the system: ");
    rvm_destroy(rvm1,  TEST_SEG "2");
    
    /*
     * segment should still not be there
     */
    cnt += TestSuccess( stat(pFileName, &statbuf) == -1 );

    /*
     * Test F: destroying an empty string doesn't destroy directory
     */
    fprintf(stdout, "Test 4f: destroying an empty segment name doesn't destroy directory: ");
    rvm_destroy(rvm1,  "");
    
    /*
     * directory should be still here
     */
    cnt += TestSuccess( stat(TEST_DIR, &statbuf) == 0 );

    /*
     * Test G: destroying an in-use segment should not work
     */
    fprintf(stdout, "Test 4g: destroying a mapped segment should fail: ");
    rvm_destroy(rvm1,  TEST_SEG);
    
    /*
     * directory should still be here
     */
    cnt += TestSuccess( stat(TEST_DIR "/" TEST_SEG, &statbuf) == 0 );

    /*
     * test H: destorying multiple segments works
     */
    fprintf(stdout, "Test 4h: destroying multiple segments succeeds: ");
    max = sizeof(testmult_diff)/sizeof(testmult_diff[0]);
    maps = testmult_diff;
    assert( (pMems1 = calloc(max, sizeof(void*))) != NULL );

    errcnt = 0;
    for(i=0; i < max; i++)
    {
        pMems1[i] = rvm_map(rvm1, maps[i].segName, maps[i].segSize);
        errcnt += TestMapCheckSuccess(pMems1[i], &maps[i], true);
    }

    for(i=0; i < max; i++)
    {
        rvm_unmap(rvm1, pMems1[i]);
    }

    for(i=0; i < max; i++)
    {
        rvm_destroy(rvm1, maps[i].segName);
        sprintf(filePath, TEST_DIR "/%s", maps[i].segName);
        if( stat(filePath, &statbuf) != -1 )
        {
            errcnt++;
        }
    }

    cnt += TestSuccess( errcnt == 0 );

    /*
     * test I: destorying multiple segments works
     */
    fprintf(stdout, "Test 4i: destroying multipe inuse segs all fail: ");
    errcnt = 0;
    for(i=0; i < max; i++)
    {
        pMems1[i] = rvm_map(rvm1, maps[i].segName, maps[i].segSize);
        errcnt += TestMapCheckSuccess(pMems1[i], &maps[i], true);
    }

    for(i=0; i < max; i++)
    {
        rvm_destroy(rvm1, maps[i].segName);
        sprintf(filePath, TEST_DIR "/%s", maps[i].segName);
        if( stat(filePath, &statbuf) == -1 )
        {
            errcnt++;
        }
    }

    cnt += TestSuccess( errcnt == 0 );

    /*
     * test J: After unmapping them they can be destroyed
     */
    fprintf(stdout, "Test 4j: after unmapping, they all now succeed: ");

    for(i=0; i < max; i++)
    {
        rvm_unmap(rvm1, pMems1[i]);
    }

    for(i=0; i < max; i++)
    {
        rvm_destroy(rvm1, maps[i].segName);
        sprintf(filePath, TEST_DIR "/%s", maps[i].segName);
        if( stat(filePath, &statbuf) != -1 )
        {
            errcnt++;
        }
    }

    cnt += TestSuccess( errcnt == 0 );

    /*
     * clean up after those two tests
     */
    free(pMems1);

    pMems1 = NULL;

    return(cnt);

} /* TestDestroy(... */

static int
Test_rvm_begin_trans()
{
    int               cnt = 0;

    fprintf(stdout, "------------ Test 5: unit tests for rvm_begin_trans() ------------\n");

    /*
     * just run all of these tests in a single child
     */
    system("rm -rf " TEST_DIR);
    cnt += RunInChild(TestBeginTrans, 0, NULL);


    return(cnt);
}

static int
TestBeginTrans( int parmcnt, void **parms )
{
    int               cnt = 0;
    int               errcnt;
    int               i;
    testseg_t       * maps;
    int               max;
    void            * pMems[2];
    void            **pMems1;
    rvm_t             rvm1;
    struct _rvm_t     testrvm;
    trans_t           txn;

    /*
     * get our handle, if we can't, no tests can be run...
     */
    if( (rvm1 = rvm_init(TEST_DIR)) == NULL )
    {
        fprintf( stdout, "Test 5 initialization - " STR_FAILED " - can't create RVM\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }

    /*
     * map a simple segment  (again for testing);
     */
    if( (pMems[0] = rvm_map(rvm1, TEST_SEG, 4096)) == NULL )
    {
        fprintf( stdout, "Test 5 initialization - " STR_FAILED " - can't map 1st seg\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }

    /*
     * map a 2nd simple segment  (again for testing);
     */
    if( (pMems[1] = rvm_map(rvm1, TEST_SEG "2", 4096)) == NULL )
    {
        fprintf( stdout, "Test 5 initialization - " STR_FAILED " - can't map 2nd seg\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }
    

    /*
     * Test A:  starting txn with NULL rvm
     */
    fprintf(stdout, "Test 5a: call with a NULL rvm fails: ");
    cnt += TestSuccess( (txn=rvm_begin_trans(NULL,  1, pMems)) == (trans_t) -1);
        
    /*
     * Test B: starting txn witn an uninitialized rvm
     */
    memset(&testrvm, '\0', sizeof(testrvm));
    fprintf(stdout, "Test 5b: start txn with an uninitialized rvm fails: ");
    cnt += TestSuccess( (txn=rvm_begin_trans(&testrvm,  1, pMems)) == (trans_t) -1);

    /*
     * Test C: with a NULL segbases name fails
     */
    fprintf(stdout, "Test 5c: start txn with a NULL segbases name fails: ");
    cnt += TestSuccess( (txn=rvm_begin_trans(rvm1,  1, NULL)) == (trans_t) -1);

    /*
     * Test D: with a zero segment count
     */
    fprintf(stdout, "Test 5d: with zero segment count fails: ");
    cnt += TestSuccess( (txn=rvm_begin_trans(rvm1,  0, pMems)) == (trans_t) -1);
    
    /*
     * Test E: with a single segment
     */
    fprintf(stdout, "Test 5e: with a single segment succeeds: ");
    cnt += TestSuccess( (txn=rvm_begin_trans(rvm1,  1, pMems)) != (trans_t) -1);

    /*
     * Test F: with same segment again fails
     */
    fprintf(stdout, "Test 5f: with same segment again fails: ");
    cnt += TestSuccess( (txn=rvm_begin_trans(rvm1,  1, pMems)) == (trans_t) -1);

    /*
     * Test G: with 2 segs one of which is already in txn fails
     */
    fprintf(stdout, "Test 5g: txn with 2 segs, one already in txn fails: ");
    cnt += TestSuccess( (txn=rvm_begin_trans(rvm1,  2, pMems)) == (trans_t) -1);

    /*
     * Test H: with 2nd only succeeds
     */
    fprintf(stdout, "Test 5h: txn with 2nd seg only succeeds: ");
    cnt += TestSuccess( (txn=rvm_begin_trans(rvm1,  1, &pMems[1])) != (trans_t) -1);

    /*
     * test I: txn with multiple segments works
     */
    fprintf(stdout, "Test 5i: txn with multiple segments succeeds: ");
    max = sizeof(testmult_diff)/sizeof(testmult_diff[0]);
    maps = testmult_diff;
    assert( (pMems1 = calloc(max, sizeof(void*))) != NULL );

    errcnt = 0;
    for(i=0; i < max; i++)
    {
        pMems1[i] = rvm_map(rvm1, maps[i].segName, maps[i].segSize);
        errcnt += TestMapCheckSuccess(pMems1[i], &maps[i], true);
    }

    
    cnt += TestSuccess(    (errcnt != 0) 
                        || ((txn=rvm_begin_trans(rvm1,  max, pMems1)) != (trans_t) -1) );


    /*
     * test J: creating txn with same multiple again fails
     */
    fprintf(stdout, "Test 5j: txn with same multiple segs fails: ");
    cnt += TestSuccess(    (errcnt != 0) 
                        || ((txn=rvm_begin_trans(rvm1,  max, pMems1)) == (trans_t) -1) );


    /*
     * test K: creating txn with one of the multiple again fails
     */
    fprintf(stdout, "Test 5j: txn with one from multiple segs (start) fails: ");
    cnt += TestSuccess(    (errcnt != 0) 
                        || ((txn=rvm_begin_trans(rvm1,  1, pMems1)) == (trans_t) -1) );

    /*
     * test L: creating txn with one of the multiple again fails
     */
    fprintf(stdout, "Test 5l: txn with one from multiple segs (end) fails: ");
    cnt += TestSuccess(    (errcnt != 0) 
                        || ((txn=rvm_begin_trans(rvm1,  1, &pMems1[max-1])) == (trans_t) -1) );

    /*
     * clean up after those two tests
     */
    free(pMems1);

    pMems1 = NULL;

    return(cnt);

} /* TestBeginTrans(... */

static int
Test_rvm_about_to_modify()
{
    int               cnt = 0;

    fprintf(stdout, "------------ Test 6: unit tests for rvm_about_to_modify() ------------\n");

    /*
     * just run all of these tests in a single child
     */
    system("rm -rf " TEST_DIR);
    cnt += RunInChild(TestModify, 0, NULL);


    return(cnt);
}

static int
TestModify( int parmcnt, void **parms )
{
    int               cnt = 0;
    char            * passed = STR_PASSED "\n";
    void            * pMems[2];
    mod_t           * pMod;
    segment_t       * pSegs;
    rvm_t             rvm1;
    trans_t           txn;
    struct _trans_t   testtxn;

    /*
     * get our handle, if we can't, no tests can be run...
     */
    if( (rvm1 = rvm_init(TEST_DIR)) == NULL )
    {
        fprintf( stdout, "Test 6 initialization - " STR_FAILED " - can't create RVM\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }

    /*
     * map a simple segment  (again for testing);
     */
    if( (pMems[0] = rvm_map(rvm1, TEST_SEG, 4096)) == NULL )
    {
        fprintf( stdout, "Test 6 initialization - " STR_FAILED " - can't map 1st seg\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }

    /*
     * map a 2nd simple segment  (again for testing);
     */
    if( (pMems[1] = rvm_map(rvm1, TEST_SEG "2", 4096)) == NULL )
    {
        fprintf( stdout, "Test 6 initialization - " STR_FAILED " - can't map 2nd seg\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }

    /*
     * create a transaction with these two segments
     */
    if( (txn=rvm_begin_trans(rvm1,  2, pMems)) == (trans_t) -1)
    {
        fprintf( stdout, "Test 6 initialization - " STR_FAILED " - can't create txn\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }

    /*
     * get the segment pointer from the txn (can't do this in real life, but for testing we can
     */
    pSegs = txn->segments;
    

    /*
     * Test A: pass in a null for tid fails
     */
    fprintf(stdout, "Test 6a: call with a NULL tid doesn't crash: ");
    rvm_about_to_modify(NULL,  pMems[0], 0, 10);

    /*
     * if we get here, we will assume we passed (no way to check without
     * poking into the internals) -- at least we know if we get here we didn't
     * cause the program to crash
     */
    fputs(passed, stdout);
        
    /*
     * Test B: using uninitialized tid
     */
    memset(&testtxn, '\0', sizeof(testtxn));
    fprintf(stdout, "Test 6b: call with an uninitialized tid doesn't crash: ");
    rvm_about_to_modify(&testtxn,  pMems[0], 10, 10);

    /*
     * if we get here, we will assume we passed (no way to check without
     * poking into the internals) -- at least we know if we get here we didn't
     * cause the program to crash
     */
    fputs(passed, stdout);

    /*
     * Test C: with a NULl segbase doesn't cause crash
     */
    fprintf(stdout, "Test 6c: call with a NULL segbase doesn't crash: ");
    rvm_about_to_modify(txn,  NULL, 10, 10);
    
    /*
     * if we get here, we will assume we passed (no way to check without
     * poking into the internals) -- at least we know if we get here we didn't
     * cause the program to crash
     */
    fputs(passed, stdout);
    

    /*
     * Test D: Let's try to create one modifyable area in the first segment
     */
    fprintf(stdout, "Test 6d: create one modifyable area in 1st segment: ");
    rvm_about_to_modify(txn,  pSegs[0]->segbase, 10, 10);
    cnt += TestSuccess( steque_size(&pSegs[0]->mods) == 1 );

    /*
     * Test E: Let's try to to repeat that
     */
    fprintf(stdout, "Test 6e: repeat just adds another mod: ");
    rvm_about_to_modify(txn,  pSegs[0]->segbase, 10, 10);
    cnt += TestSuccess( steque_size(&pSegs[0]->mods) == 2 );


    /*
     * Test F: extending the area is ignored
     */
    fprintf(stdout, "Test 6f: extending area adds another mod: ");
    rvm_about_to_modify(txn,  pSegs[0]->segbase, 10, 100);
    cnt += TestSuccess( steque_size(&pSegs[0]->mods) == 3 );

    /*
     * Test G: adding another area works
     */
    fprintf(stdout, "Test 6g: adding another area succeeds: ");
    rvm_about_to_modify(txn,  pSegs[0]->segbase, 1024, 100);
    cnt += TestSuccess( steque_size(&pSegs[0]->mods) == 4 );

    /*
     * Test H: adding an area in the 2nd segment works
     */
    fprintf(stdout, "Test 6h: adding area in 2nd segment succeeds: ");
    rvm_about_to_modify(txn,  pSegs[1]->segbase, 0, 10);
    cnt += TestSuccess(    (steque_size(&pSegs[0]->mods) == 4)
                        && (steque_size(&pSegs[1]->mods) == 1) );


    return(cnt);

} /* TestModify(... */

static int
RunInChild( int (*func)(), int cnt, void **parms)
{
    extern int        errno;
    int               exitstatus;
    pid_t             pid;
    int               rtn = 0;

    /*
     * in the child, run the function...
     */
    if( (pid=fork()) == 0 )
    {
        exit(func(cnt, parms) );
    }
    /*
     * if the fork failed
     */
    else if( pid == -1 )
    {
        fprintf(stderr,"Internal error:  Fork Failed, errno=%d\n", errno);
        exit(20);
    }
    else
    {
        if( waitpid(pid, &exitstatus,0) == -1 )
        {
            fprintf(stderr,"Internal error: Faild to wait on child process, errno=%d\n", errno);
            exit(20);
        }
        if( (! WIFEXITED(exitstatus)) || (WEXITSTATUS(exitstatus) != 0) )
        {
            if( exitOnError )
            {
                exit(10);
            }
            rtn++;
        }
    }

    return(rtn);

} /* RunInChild(... */

static void
FillInData(char * pName, char * pData, size_t cnt)
{
    int         fd;

    assert( (fd = open(pName, O_RDWR)) != -1);

    assert( write(fd, pData, cnt) == cnt );

    assert( close(fd) != -1 );
}

static int 
TestSuccess(bool success )
{
    int               cnt = 0;
    char            * failed = STR_FAILED "\n";
    char            * passed = STR_PASSED "\n";

    if( success )
    {
        fputs(passed, stdout);
    }
    else
    {
        fputs(failed,stdout);
        if( exitOnError )
        {
            exit(10);
        }
        cnt++;
    }

    return(cnt);

} /* TestSuccess(... */


