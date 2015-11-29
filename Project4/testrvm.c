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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
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

/*
 * Definitions for load tests
 */
#define PARMS_LOAD_SEGS             0
#define PARMS_LOAD_UPS_PER_TXN      1
#define PARMS_LOAD_MAX_CNT          2
#define PARMS_LOAD_SIZE             3

static void FillInData(char * pName, char * pData, size_t cnt);
static int  RunInChild( int (*func)(int cnt, void **parms), int cnt, void **parms);
static int  InterruptedRunInChild( int (*func)(), long sleepTime, int cnt, void **parms);
static int  Test_rvm_init();
static int  Test_rvm_map();
static int  Test_rvm_unmap();
static int  Test_rvm_destroy();
static int  Test_rvm_begin_trans();
static int  Test_rvm_about_to_modify();
static int  Test_rvm_abort_trans();
static int  Test_rvm_commit_trans();
static int  Test_rvm_truncate_log();
static int  Test_integ();
static int  Test_load();
static int  TestAbort( int parmcnt, void **parms );
static int  TestCommit( int parmcnt, void **parms );
static int  TestModify( int parmcnt, void **parms );
static int  TestMultiInit(int cnt, void **parms);
static int  TestBeginTrans( int parmcnt, void **parms );
static int  TestDestroy( int parmcnt, void **parms );
static int  TestGoodInit(int cnt, void **parms);
static int  TestMapCheckSuccess(void * pMem, testseg_t * pTest, bool silent);
static int  TestMapMultiple(int cnt, void **parms);
static int  TestUnmap( int cnt, void **parms );
static int  TestSuccess(bool success );
static int  TestTruncate( int parmcnt, void **parms );
static int  TestInteg( int parmcnt, void **parms );
static int  TestAbortPartialChange( int parmcnt, void **parms );
static int  TestLoad( int parmcnt, void **parms );


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
     * Test 1: test rvm_init...
     */
    cnt += Test_rvm_init();

    /*
     * Test 2: test rvm_map...
     */
    cnt += Test_rvm_map();

    /*
     * Test 3: test rvm_unmap()...
     */
    cnt += Test_rvm_unmap();

    /*
     * Test 4: test rvm_destroy()...
     */
    cnt += Test_rvm_destroy();

    /*
     * Test 5: test rvm_begin_trans()...
     */
    cnt += Test_rvm_begin_trans();

    /*
     * Test 6: test rvm_about_to_modify()...
     */
    cnt += Test_rvm_about_to_modify();

    /*
     * Test 7: test rvm_abort_trans()...
     */
    cnt += Test_rvm_abort_trans();

    /*
     * Test 8: test rvm_commit_trans()...
     */
    cnt += Test_rvm_commit_trans();

    /*
     * Test 9: test rvm_truncate_log()...
     */
    cnt += Test_rvm_truncate_log();

    /*
     * Test 10: integration tests()...
     */
    cnt += Test_integ();

    /*
     * Test 11: Load test of overall system
     */
    cnt += Test_load();

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
                rvm_unmap(rvm1, pMem[i]);
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
    cnt += TestSuccess( (pMem2 = rvm_map(rvm1, TEST_SEG, 4096)) == NULL );

    /*
     * Test E: off by 1 pointer fails
     */
    fprintf(stdout, "Test 3e: call with off by 1 byte (+) pointer fails: ");
    rvm_unmap(rvm1,  pMem+1);
    cnt += TestSuccess( (pMem2 = rvm_map(rvm1, TEST_SEG, 4096)) == NULL );

    /*
     * Test F: off by 1 byte (-) pointer fails
     */
    fprintf(stdout, "Test 3f: call with off by 1 byte (-) pointer fails: ");
    rvm_unmap(rvm1,  pMem-1);
    cnt += TestSuccess( (pMem2 = rvm_map(rvm1, TEST_SEG, 4096)) == NULL );

    /*
     * Test G: verify we can map again after unmap (Note: this one is correctly replacing pMem)
     */
    fprintf(stdout, "Test 3g: map again after unmap succeeds: ");
    rvm_unmap(rvm1,  pMem);
    cnt += TestSuccess( (pMem = rvm_map(rvm1, TEST_SEG, 4096)) != NULL );

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
    segment_t       * pSegs;
    rvm_t             rvm1;
    int               size;
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
     * Test E: repeating change doesn't add another mod
     *          allow both behaviors
     */
    fprintf(stdout, "Test 6e: repeat doesn't add another mod: ");
    rvm_about_to_modify(txn,  pSegs[0]->segbase, 10, 10);
    size = steque_size(&pSegs[0]->mods);
    cnt += TestSuccess( (size == 1) || (size == 2) );


    /*
     * Test F: extending the area adds another mod
     */
    fprintf(stdout, "Test 6f: extending area adds another mod: ");
    rvm_about_to_modify(txn,  pSegs[0]->segbase, 10, 100);
    cnt += TestSuccess( steque_size(&pSegs[0]->mods) == (size+1) );

    /*
     * Test G: adding another area works
     */
    fprintf(stdout, "Test 6g: adding another area succeeds: ");
    rvm_about_to_modify(txn,  pSegs[0]->segbase, 1024, 100);
    cnt += TestSuccess( steque_size(&pSegs[0]->mods) == (size+2) );

    /*
     * Test H: adding an area in the 2nd segment works
     */
    fprintf(stdout, "Test 6h: adding area in 2nd segment succeeds: ");
    rvm_about_to_modify(txn,  pSegs[1]->segbase, 0, 10);
    cnt += TestSuccess(    (steque_size(&pSegs[0]->mods) == (size+2))
                        && (steque_size(&pSegs[1]->mods) == 1) );


    return(cnt);

} /* TestModify(... */

static int
Test_rvm_abort_trans()
{
    int               cnt = 0;

    fprintf(stdout, "------------ Test 7: unit tests for rvm_abort_trans() ------------\n");

    /*
     * just run all of these tests in a single child
     */
    system("rm -rf " TEST_DIR);
    cnt += RunInChild(TestAbort, 0, NULL);

    return(cnt);
}

static int
TestAbort( int parmcnt, void **parms )
{
    int               cnt = 0;
    char            * passed = STR_PASSED "\n";
    void            * pMems[2];
    segment_t         pSeg;
    segment_t         pSeg2;
    rvm_t             rvm1;
    char            * s;
    char            * s2;
    trans_t           txn;
    struct _trans_t   testtxn;

    /*
     * get our handle, if we can't, no tests can be run...
     */
    system("rm -rf " TEST_DIR );
    if( (rvm1 = rvm_init(TEST_DIR)) == NULL )
    {
        fprintf( stdout, "Test 7 initialization - " STR_FAILED " - can't create RVM\n");
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
        fprintf( stdout, "Test 7 initialization - " STR_FAILED " - can't map 1st seg\n");
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
        fprintf( stdout, "Test 7 initialization - " STR_FAILED " - can't map 2nd seg\n");
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
        fprintf( stdout, "Test 7 initialization - " STR_FAILED " - can't create txn\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }

    /*
     * Test A: pass in a null for tid fails
     */
    fprintf(stdout, "Test 7a: call with a NULL tid doesn't crash: ");
    rvm_abort_trans(NULL);

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
    fprintf(stdout, "Test 7b: call with an uninitialized tid doesn't crash: ");
    rvm_abort_trans(&testtxn);

    /*
     * if we get here, we will assume we passed (no way to check without
     * poking into the internals) -- at least we know if we get here we didn't
     * cause the program to crash
     */
    fputs(passed, stdout);

    /*
     * Test C: abort txn with no changes is OK
     */
    fprintf(stdout, "Test 7c: abort with no changes succeeds: "); fflush(stdout);
    pSeg = txn->segments[0];                // get this before we abort (it goes away)
    pSeg2 = txn->segments[1];
    rvm_abort_trans(txn);

    cnt += TestSuccess( (pSeg->cur_trans == NULL) && (pSeg2->cur_trans == NULL) );


    /*
     * Test D: Can't use transaction again
     */
    fprintf(stdout, "Test 7d: using aborted txn fails: ");
    fflush(stdout);
    rvm_about_to_modify(txn,  pSeg->segbase, 10, 10);   // should not work, txn was aborted
    fflush(stdout);
    ((char *) (pSeg->segbase))[10] = 'a';
    rvm_abort_trans(txn);
    cnt += TestSuccess( ((char *) (pSeg->segbase))[10] == 'a');
    ((char *) (pSeg->segbase))[10] = '\0';

    /*
     * Test E: transaction abort does restore data
     */
    fprintf(stdout, "Test 7e: abort does restore data: ");
    assert( (txn = rvm_begin_trans(rvm1,  2, pMems)) != (trans_t) -1);
    pSeg = txn->segments[0];                  // get this before we abort (it goes away)
    rvm_about_to_modify(txn,  pSeg->segbase, 10, 10);
    ((char *) (pSeg->segbase))[10] = 'a';
    rvm_abort_trans(txn);
    cnt += TestSuccess( ((char *) (pSeg->segbase))[10] == '\0');


    /*
     * Test F: aborting multiple overlapping regions works correctly
     */
    fprintf(stdout, "Test 7f: abort multiple overlapping regions works: ");
    assert( (txn = rvm_begin_trans(rvm1,  2, pMems)) != (trans_t) -1);
    pSeg = txn->segments[0];                  // get this before we abort (it goes away)
    s  = (char *) pSeg->segbase;
    strcpy(s,  "Hello world, Conor is Here");
    rvm_about_to_modify(txn,  pSeg->segbase, 0, 10);
    memcpy(s+1, "ELLO", 4);
    rvm_about_to_modify(txn,  pSeg->segbase, 3, 10);
    s[3] = 'K';
    rvm_about_to_modify(txn,  pSeg->segbase, 10, 17);
    memcpy(s+13, "No He Isn't", 11);
    rvm_about_to_modify(txn,  pSeg->segbase, 0, 10);
    memcpy(s, "je", 2);

    /*
     * make sure our changes took
     */
    assert( strcmp(s, "jeLKO world, No He Isn'tre") == 0 );

    rvm_abort_trans(txn);

    cnt += TestSuccess( strcmp(s, "Hello world, Conor is Here") == 0 );

    /*
     * Test G: aborting changes in multiple segments works
     */
    fprintf(stdout, "Test 7g: abort changes in multiple segments works: ");
    assert( (txn = rvm_begin_trans(rvm1,  2, pMems)) != (trans_t) -1);
    pSeg = txn->segments[0];                  // get this before we abort (it goes away)
    s  = (char *) pSeg->segbase;

    pSeg2 = txn->segments[1];
    s2 = (char *) pSeg2->segbase;
    strcpy(s,  "Hello world, Conor is Here");
    strcpy(s2, "Goodbye World, Conor Left a while ago");

    rvm_about_to_modify(txn,  pSeg->segbase, 0, 20);
    memcpy(s, "12345678901234567890",20);
    rvm_about_to_modify(txn,  pSeg2->segbase, 0, 20);
    memcpy(s2, "12345678901234567890",20);

    /*
     * make sure our changes took
     */
    assert( strcmp(s, "12345678901234567890s Here") == 0 );
    assert( strcmp(s2, "12345678901234567890 Left a while ago") == 0);

    rvm_abort_trans(txn);

    cnt += TestSuccess( (strcmp(s,  "Hello world, Conor is Here") == 0)
                        && (strcmp(s2, "Goodbye World, Conor Left a while ago") == 0) );



    return(cnt);

} /* TestAbort(... */

static int
Test_rvm_commit_trans()
{
    int               cnt = 0;

    fprintf(stdout, "------------ Test 8: unit tests for rvm_commit_trans() ------------\n");

    /*
     * just run all of these tests in a single child
     */
    system("rm -rf " TEST_DIR);
    cnt += RunInChild(TestCommit, 0, NULL);

    return(cnt);
}

static int
TestCommit( int parmcnt, void **parms )
{
    int               cnt = 0;
    char            * passed = STR_PASSED "\n";
    void            * pMems[2];
    segment_t         pSeg;
    segment_t         pSeg2;
    rvm_t             rvm1;
    char            * s;
    char            * s2;
    trans_t           txn;
    struct _trans_t   testtxn;

    /*
     * get our handle, if we can't, no tests can be run...
     */
    system("rm -rf " TEST_DIR );
    if( (rvm1 = rvm_init(TEST_DIR)) == NULL )
    {
        fprintf( stdout, "Test 8 initialization - " STR_FAILED " - can't create RVM\n");
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
        fprintf( stdout, "Test 8 initialization - " STR_FAILED " - can't map 1st seg\n");
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
        fprintf( stdout, "Test 8 initialization - " STR_FAILED " - can't map 2nd seg\n");
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
        fprintf( stdout, "Test 8 initialization - " STR_FAILED " - can't create txn\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }

    /*
     * Test A: pass in a null for tid fails
     */
    fprintf(stdout, "Test 8a: call with a NULL tid doesn't crash: ");
    rvm_commit_trans(NULL);

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
    fprintf(stdout, "Test 8b: call with an uninitialized tid doesn't crash: ");
    rvm_commit_trans(&testtxn);

    /*
     * if we get here, we will assume we passed (no way to check without
     * poking into the internals) -- at least we know if we get here we didn't
     * cause the program to crash
     */
    fputs(passed, stdout);

    /*
     * Test C: commit txn with no changes is OK
     */
    fprintf(stdout, "Test 8c: commit with no changes succeeds: "); fflush(stdout);
    pSeg = txn->segments[0];                // get this before we abort (it goes away)
    pSeg2 = txn->segments[1];
    rvm_commit_trans(txn);

    cnt += TestSuccess( (pSeg->cur_trans == NULL) && (pSeg2->cur_trans == NULL) );


    /*
     * Test D: Can't commit an aborted tranaction
     */
    fprintf(stdout, "Test 8d: can't commit an aborted txn: ");
    assert( (txn = rvm_begin_trans(rvm1,  2, pMems)) != (trans_t) -1);
    ((char *) (pSeg->segbase))[10] = 'a';
    rvm_about_to_modify(txn,  pSeg->segbase, 10, 10);   // should not work, txn was aborted
    ((char *) (pSeg->segbase))[10] = 'b';
    rvm_abort_trans(txn);
    rvm_commit_trans(txn);
    cnt += TestSuccess( ((char *) (pSeg->segbase))[10] == 'a');

    /*
     * Test E: can't abort after commit
     */
    fprintf(stdout, "Test 8e: can't abort after commit: ");
    assert( (txn = rvm_begin_trans(rvm1,  2, pMems)) != (trans_t) -1);
    ((char *) (pSeg->segbase))[10] = 'a';
    rvm_about_to_modify(txn,  pSeg->segbase, 10, 10);   // should not work, txn was aborted
    ((char *) (pSeg->segbase))[10] = 'b';
    rvm_commit_trans(txn);
    rvm_abort_trans(txn);
    cnt += TestSuccess( ((char *) (pSeg->segbase))[10] == 'b');


    /*
     * Test F: committing multiple overlapping regions works (as far as we can tell --
     *         need full integ tests to know for sure)
     */
    fprintf(stdout, "Test 8f: commit multiple overlapping regions works: ");
    assert( (txn = rvm_begin_trans(rvm1,  2, pMems)) != (trans_t) -1);
    pSeg = txn->segments[0];                  // get this before we abort (it goes away)
    pSeg2 = txn->segments[1];
    s  = (char *) pSeg->segbase;
    strcpy(s,  "Hello world, Conor is Here");
    rvm_about_to_modify(txn,  pSeg->segbase, 0, 10);
    memcpy(s+1, "ELLO", 4);
    rvm_about_to_modify(txn,  pSeg->segbase, 3, 10);
    s[3] = 'K';
    rvm_about_to_modify(txn,  pSeg->segbase, 10, 17);
    memcpy(s+13, "No He Isn't", 11);
    rvm_about_to_modify(txn,  pSeg->segbase, 0, 10);
    memcpy(s, "je", 2);

    /*
     * make sure our changes took
     */
    assert( strcmp(s, "jeLKO world, No He Isn'tre") == 0 );

    rvm_commit_trans(txn);

    cnt += TestSuccess(    (strcmp(s, "jeLKO world, No He Isn'tre") == 0)
                        && (pSeg->cur_trans == NULL)
                        && (pSeg2->cur_trans == NULL) );

    /*
     * Test G: committing changes in multiple segments works
     */
    fprintf(stdout, "Test 8g: commit changes in multiple segments works: ");
    assert( (txn = rvm_begin_trans(rvm1,  2, pMems)) != (trans_t) -1);
    pSeg = txn->segments[0];                  // get this before we abort (it goes away)
    s  = (char *) pSeg->segbase;

    pSeg2 = txn->segments[1];
    s2 = (char *) pSeg2->segbase;
    strcpy(s,  "Hello world, Conor is Here");
    strcpy(s2, "Goodbye World, Conor Left a while ago");

    rvm_about_to_modify(txn,  pSeg->segbase, 0, 40);
    memcpy(s, "12345678901234567890",20);
    rvm_about_to_modify(txn,  pSeg2->segbase, 0, 40);
    memcpy(s2, "12345678901234567890",20);

    /*
     * make sure our changes took
     */
    assert( strcmp(s, "12345678901234567890s Here") == 0 );
    assert( strcmp(s2, "12345678901234567890 Left a while ago") == 0);

    rvm_commit_trans(txn);

    cnt += TestSuccess(    (strcmp(s,"12345678901234567890s Here") == 0)
                        && (strcmp(s2,"12345678901234567890 Left a while ago") == 0)
                        && (pSeg->cur_trans == NULL)
                        && (pSeg2->cur_trans == NULL) );



    return(cnt);

} /* TestCommit(... */

static int
Test_rvm_truncate_log()
{
    int               cnt = 0;

    fprintf(stdout, "------------ Test 9: unit tests for rvm_truncate_log() ------------\n");

    /*
     * just run all of these tests in a single child
     */
    system("rm -rf " TEST_DIR);
    cnt += RunInChild(TestTruncate, 0, NULL);

    return(cnt);
}

static int
TestTruncate( int parmcnt, void **parms )
{
    char              buf1[512];
    char              buf2[512];
    int               cnt = 0;
    int               fd1;
    int               fd2;
    char            * passed = STR_PASSED "\n";
    void            * pMems[2];
    segment_t         pSeg;
    segment_t         pSeg2;
    rvm_t             rvm1;
    char            * s;
    char            * s2;
    struct _rvm_t     testrvm;
    trans_t           txn;

    /*
     * get our handle, if we can't, no tests can be run...
     */
    system("rm -rf " TEST_DIR );
    if( (rvm1 = rvm_init(TEST_DIR)) == NULL )
    {
        fprintf( stdout, "Test 9 initialization - " STR_FAILED " - can't create RVM\n");
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
        fprintf( stdout, "Test 9 initialization - " STR_FAILED " - can't map 1st seg\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }
    assert( (fd1 = open( TEST_DIR "/" TEST_SEG, O_RDWR )) != -1);

    /*
     * map a 2nd simple segment  (again for testing);
     */
    if( (pMems[1] = rvm_map(rvm1, TEST_SEG "2", 4096)) == NULL )
    {
        fprintf( stdout, "Test 9 initialization - " STR_FAILED " - can't map 2nd seg\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }
    assert( (fd2 = open( TEST_DIR "/" TEST_SEG "2", O_RDWR )) != -1);

    /*
     * create a transaction with these two segments
     */
    if( (txn=rvm_begin_trans(rvm1,  2, pMems)) == (trans_t) -1)
    {
        fprintf( stdout, "Test 9 initialization - " STR_FAILED " - can't create txn\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }

    /*
     * Test A: pass in a null for tid fails
     */
    fprintf(stdout, "Test 9a: call with a NULL rvm doesn't crash: ");
    rvm_truncate_log(NULL);

    /*
     * if we get here, we will assume we passed (no way to check without
     * poking into the internals) -- at least we know if we get here we didn't
     * cause the program to crash
     */
    fputs(passed, stdout);

    /*
     * Test B: using uninitialized tid
     */
    memset(&testrvm, '\0', sizeof(testrvm));
    fprintf(stdout, "Test 9b: call with an uninitialized rvm doesn't crash: ");
    rvm_truncate_log(&testrvm);

    /*
     * if we get here, we will assume we passed (no way to check without
     * poking into the internals) -- at least we know if we get here we didn't
     * cause the program to crash
     */
    fputs(passed, stdout);

    /*
     * Test C: commit txn with no changes is OK
     */
    fprintf(stdout, "Test 9c: truncate with no changes works: "); fflush(stdout);
    pSeg = txn->segments[0];                  // get this before we abort (it goes away)
    pSeg2 = txn->segments[1];
    rvm_commit_trans(txn);
    rvm_truncate_log(rvm1);

    cnt += TestSuccess( (pSeg->cur_trans == NULL) && (pSeg2->cur_trans == NULL) );


    /*
     * Test D: committed change without trucate does not show up
     */
    fprintf(stdout, "Test 9d: no visible change without truncate: ");
    assert( (txn = rvm_begin_trans(rvm1,  2, pMems)) != (trans_t) -1);
    rvm_about_to_modify(txn,  pMems[0], 0, 28);   // should not work, txn was aborted
    memcpy(pMems[0], "Hello world, Conor is Here\n", 28);
    rvm_commit_trans(txn);
    assert(lseek(fd1,0L,SEEK_SET) == 0 );
    assert(read(fd1, buf1, 28) == 28 );
    cnt += TestSuccess( strcmp(buf1, "Hello world, Conor is Here\n") != 0 );

    /*
     * Test E: data is visible after truncation
     */
    fprintf(stdout, "Test 9e: but it is visible after truncation: ");
    rvm_truncate_log(rvm1);
    assert(lseek(fd1,0L,SEEK_SET) == 0 );
    assert(read(fd1, buf1, 28) == 28 );
    cnt += TestSuccess( strcmp(buf1, "Hello world, Conor is Here\n") == 0 );


    /*
     * Test F: committing multiple overlapping regions works (as far as we can tell --
     *         need full integ tests to know for sure)
     */
    fprintf(stdout, "Test 9f: commit multiple overlapping regions works: ");
    assert( (txn = rvm_begin_trans(rvm1,  2, pMems)) != (trans_t) -1);
    s  = (char *) pMems[0];
    rvm_about_to_modify(txn,  pMems[0], 0, 10);
    memcpy(s+1, "ELLO", 4);
    rvm_about_to_modify(txn,  pMems[0], 3, 10);
    s[3] = 'K';
    rvm_about_to_modify(txn,  pMems[0], 10, 17);
    memcpy(s+13, "No He Isn't", 11);
    rvm_about_to_modify(txn,  pMems[0], 0, 10);
    memcpy(s, "je", 2);

    /*
     * make sure our changes took
     */
    assert( strcmp(s, "jeLKO world, No He Isn'tre\n") == 0 );

    rvm_commit_trans(txn);
    rvm_truncate_log(rvm1);
    assert(lseek(fd1,0L,SEEK_SET) == 0 );
    assert(read(fd1, buf1, 28) == 28 );

    cnt += TestSuccess(  strcmp(buf1, "jeLKO world, No He Isn'tre\n") == 0);

    /*
     * Test G: committing changes in multiple segments works
     */
    fprintf(stdout, "Test 9g: changes in multiple segments works: ");
    assert( (txn = rvm_begin_trans(rvm1,  2, pMems)) != (trans_t) -1);
    s  = (char *) pMems[0];
    s2 = (char *) pMems[1];
    rvm_about_to_modify(txn,  pMems[0], 0, 40);
    strcpy(s,  "Hello World  Conor was Here");
    rvm_about_to_modify(txn,  pMems[1], 0, 40);
    strcpy(s2, "Goodbye World, Conor Left a while ago");
    rvm_commit_trans(txn);
    rvm_truncate_log(rvm1);

    assert(lseek(fd1,0L,SEEK_SET) == 0 );
    assert(read(fd1, buf1, 40) == 40 );
    assert(lseek(fd2,0L,SEEK_SET) == 0 );
    assert(read(fd2, buf2, 40) == 40 );

    cnt += TestSuccess(    (strcmp(buf1, "Hello World  Conor was Here") == 0 )
                        && (strcmp(buf2, "Goodbye World, Conor Left a while ago") == 0) );


    /*
     * Test H: multiple commited transactons in multiple segments works
     */
    fprintf(stdout, "Test 9h: multiple changes in multiple segments works: ");
    assert( (txn = rvm_begin_trans(rvm1,  2, pMems)) != (trans_t) -1);
    rvm_about_to_modify(txn,  pMems[0], 0, 10);
    memcpy(s, "1234567890",10);
    rvm_about_to_modify(txn,  pMems[1], 0, 10);
    memcpy(s2, "1234567890",10);
    rvm_commit_trans(txn);

    assert( (txn = rvm_begin_trans(rvm1,  2, pMems)) != (trans_t) -1);
    rvm_about_to_modify(txn,  pMems[0], 10, 10);
    memcpy(s+10,  "1234567890",10);
    rvm_about_to_modify(txn,  pMems[1], 10, 10);
    memcpy(s2+10, "1234567890",10);
    rvm_commit_trans(txn);

    /*
     * make sure our changes took
     */
    assert( strcmp(s, "12345678901234567890as Here") == 0 );
    assert( strcmp(s2, "12345678901234567890 Left a while ago") == 0);

    rvm_truncate_log(rvm1);

    assert(lseek(fd1,0L,SEEK_SET) == 0 );
    assert(read(fd1, buf1, 28) == 28 );

    assert(lseek(fd1,0L,SEEK_SET) == 0 );
    assert(read(fd1, buf1, 40) == 40 );
    assert(lseek(fd2,0L,SEEK_SET) == 0 );
    assert(read(fd2, buf2, 40) == 40 );
    cnt += TestSuccess(    (strcmp(buf1,"12345678901234567890as Here") == 0)
                        && (strcmp(buf2,"12345678901234567890 Left a while ago") == 0) );



    return(cnt);

} /* TestTruncate(... */

static int
Test_integ()
{
    int               cnt = 0;

    fprintf(stdout, "------------ Test 10: integratin tests ------------\n");

    /*
     * just run all of these tests in a single child
     */
    system("rm -rf " TEST_DIR);
    cnt += RunInChild(TestInteg, 0, NULL);

    return(cnt);
}

static int
TestInteg( int parmcnt, void **parms )
{
    int               cnt = 0;
    int               i;
    void            * params[4];
    char            * pMems[2];
    rvm_t             rvm1;
    trans_t           txn;

    /*
     * get our handle, if we can't, no tests can be run...
     */
    system("rm -rf " TEST_DIR );
    if( (rvm1 = rvm_init(TEST_DIR)) == NULL )
    {
        fprintf( stdout, "Test 9 initialization - " STR_FAILED " - can't create RVM\n");
        if( exitOnError )
        {
            exit(10);
        }
        return(1);
    }

    /*
     * Test A: Segments are initialized to all NULL bytes
     */
    fprintf(stdout, "Test 10a: Segment initialized as all nulls: ");
    assert((pMems[0] = rvm_map(rvm1, TEST_SEG, 4096)) != NULL );

    for(i=0; i < 4096; i++)
    {
        if( pMems[0][i] != '\0' )
        {
            break;
        }
    }
    cnt += TestSuccess( i == 4096 );

    /*
     * Test B: unmap and remap of same segment still full of nulls
     */
    fprintf(stdout, "Test 10b: Segment still nulls after unmap and remap: ");

    rvm_unmap(rvm1, pMems[0]);
    assert((pMems[0] = rvm_map(rvm1, TEST_SEG, 4096)) != NULL );
    for(i=0; i < 4096; i++)
    {
        if( pMems[0][i] != '\0' )
        {
            break;
        }
    }
    cnt += TestSuccess( i == 4096 );

    /*
     * Test C: change data with no rvm_about_to_modify, unmap and remap of same segment still full of nulls
     */
    fprintf(stdout, "Test 10c: Changed data without rvm_about_to_modify is lost on remap: ");

    strcpy(pMems[0], "Hello World!\n");

    rvm_unmap(rvm1, pMems[0]);
    assert((pMems[0] = rvm_map(rvm1, TEST_SEG, 4096)) != NULL );
    for(i=0; i < 4096; i++)
    {
        if( pMems[0][i] != '\0' )
        {
            break;
        }
    }
    cnt += TestSuccess( i == 4096 );


    /*
     * Test D: change data rvm_about_to_modify but no commit, unmap and remap of same segment still full of nulls
     */
    fprintf(stdout, "Test 10d: Changed data without commit is lost on remap: ");
    fflush(stdout);

    params[0] = (void *) rvm1;
    params[1] = (void *) pMems;

    RunInChild( TestAbortPartialChange, 2, (void **)params);

    rvm_unmap(rvm1, pMems[0]);
    rvm_truncate_log(rvm1);
    assert((pMems[0] = rvm_map(rvm1, TEST_SEG, 4096)) != NULL );
    for(i=0; i < 4096; i++)
    {
        if( pMems[0][i] != '\0' )
        {
            break;
        }
    }
    cnt += TestSuccess( i == 4096 );

    /*
     * Test E: changed data, aborted txn, unmap and remap of same segment still full of nulls
     */
    fprintf(stdout, "Test 10e: Changed data with abort is lost on remap: ");
    fflush(stdout);

    assert( (txn = rvm_begin_trans(rvm1,  1, (void**)pMems)) != (trans_t) -1);
    rvm_about_to_modify(txn,  pMems[0], 0, 32);
    strcpy(pMems[0], "Hello World!\n");
    rvm_abort_trans(txn);

    rvm_unmap(rvm1, pMems[0]);
    rvm_truncate_log(rvm1);
    assert((pMems[0] = rvm_map(rvm1, TEST_SEG, 4096)) != NULL );
    for(i=0; i < 4096; i++)
    {
        if( pMems[0][i] != '\0' )
        {
            break;
        }
    }
    cnt += TestSuccess( i == 4096 );

    /*
     * Test F: committed data is present after remap
     */
    fprintf(stdout, "Test 10f: committed data is present after remap: ");
    fflush(stdout);

    assert( (txn = rvm_begin_trans(rvm1,  1, (void**)pMems)) != (trans_t) -1);
    rvm_about_to_modify(txn,  pMems[0], 0, 32);
    strcpy(pMems[0], "Hello World!\n");
    rvm_commit_trans(txn);

    rvm_unmap(rvm1, pMems[0]);

    assert((pMems[0] = rvm_map(rvm1, TEST_SEG, 4096)) != NULL );
    for(i=strlen("Hello world!\n"); i < 4096; i++)
    {
        if( pMems[0][i] != '\0' )
        {
            break;
        }
    }
    cnt += TestSuccess( (strcmp(pMems[0], "Hello World!\n") == 0) && (i == 4096) );

    /*
     * Test G: Extended data is also filled with nulls and the data that was
     *         in the segment is still there
     */
    fprintf(stdout, "Test 10g: Extended data is filled with nulls and no lost data: ");
    fflush(stdout);

    rvm_unmap(rvm1, pMems[0]);
    rvm_truncate_log(rvm1);
    assert((pMems[0] = rvm_map(rvm1, TEST_SEG, 4096*2)) != NULL );
    for(i=strlen("Hello world!\n"); i < 4096*2; i++)
    {
        if( pMems[0][i] != '\0' )
        {
            break;
        }
    }
    cnt += TestSuccess( (strcmp(pMems[0], "Hello World!\n") == 0) && (i == 4096*2) );


    return(cnt);

} /* TestInteg(... */

static int
TestAbortPartialChange( int parmcnt, void **params )
{
    trans_t           txn;
    void            **pMems = (void **) params[1];

    assert( (txn = rvm_begin_trans(params[0],  1, (void**)pMems)) != (trans_t) -1);
    rvm_about_to_modify(txn,  pMems[0], 0, 32);
    strcpy(pMems[0], "Hello World!\n");

    exit(0);

} /* TestAbortPartialChange(... */

typedef struct LoadDef
{
    int           segSize;              // the size of the segments to create
    int           numCountIncs;;        // number of counter increments per segment
    int           numSegs;              // number of segments to create and use
    int           numOpsPerTxn;         // number of counter updates per each transaction
    int           numTxnsPerTrunc;      // number of transactions between truncations
    int           intOffset;            // how far in the segment to put the ints
} LoadDef_t;

static int
Test_load()
{
    int               cnt = 0;
    LoadDef_t         loadParms;
    void            * parms[2];
//  char            * failed = STR_FAILED "\n";
//  char            * passed = STR_PASSED "\n";
    long              sleepTime;
    struct timeval    tend;
    struct timeval    tstart;

    fprintf(stdout, "------------ Test 11: load tests ------------\n");

    /*
     * Test A: simple count to 1000 in a single segment
     */
    fprintf(stdout, "Test 11a: 1 seg, 4k size, 0k offt,  1000 cnt, 1/1/1 trunc/txn/op: ");
    fflush(stdout);
    system("rm -rf " TEST_DIR);
    loadParms.segSize = 4096;
    loadParms.numCountIncs=1000;
    loadParms.numSegs=1;
    loadParms.numOpsPerTxn = 1;
    loadParms.numTxnsPerTrunc = 1;
    loadParms.intOffset = 0;
    parms[0] = (void *) &loadParms;
    cnt += RunInChild(TestLoad, 1, parms);

    /*
     * Test B:
     */
    fprintf(stdout, "Test 11b: 1 seg, 4k size, 1k offt,  1000 cnt, 1/1/1 trunc/txn/op: ");
    fflush(stdout);
    system("rm -rf " TEST_DIR);
    loadParms.segSize = 4096;
    loadParms.numCountIncs=1000;
    loadParms.numSegs=1;
    loadParms.numOpsPerTxn = 1;
    loadParms.numTxnsPerTrunc = 1;
    loadParms.intOffset = 1024;
    parms[0] = (void *) &loadParms;
    cnt += RunInChild(TestLoad, 1, parms);

    /*
     * Test C:
     */
    fprintf(stdout, "Test 11c: 1 seg, 40k size, 1k offt,  1000 cnt, 1/1/1 trunc/txn/op: ");
    fflush(stdout);
    system("rm -rf " TEST_DIR);
    loadParms.segSize = 40*1024;
    loadParms.numCountIncs=1000;
    loadParms.numSegs=1;
    loadParms.numOpsPerTxn = 1;
    loadParms.numTxnsPerTrunc = 1;
    loadParms.intOffset = 10*1024;
    parms[0] = (void *) &loadParms;
    cnt += RunInChild(TestLoad, 1, parms);

    /*
     * Test D:
     */
    fprintf(stdout, "Test 11d: 6 segs, 4k size, 2k offt,  1000 cnt, 1/1/1 trunc/txn/op: ");
    fflush(stdout);
    system("rm -rf " TEST_DIR);
    loadParms.segSize = 4096;
    loadParms.numCountIncs=1000;
    loadParms.numSegs=6;
    loadParms.numOpsPerTxn = 1;
    loadParms.numTxnsPerTrunc = 1;
    loadParms.intOffset = 2*1024;
    parms[0] = (void *) &loadParms;
    cnt += RunInChild(TestLoad, 1, parms);

    /*
     * Test E:
     */
    fprintf(stdout, "Test 11e: 6 segs, 100k size, 2k offt,  1000 cnt, 1/1/1 trunc/txn/op: ");
    fflush(stdout);
    system("rm -rf " TEST_DIR);
    loadParms.segSize = 100*1024;
    loadParms.numCountIncs=1000;
    loadParms.numSegs=6;
    loadParms.numOpsPerTxn = 1;
    loadParms.numTxnsPerTrunc = 1;
    loadParms.intOffset = 2*1024;
    parms[0] = (void *) &loadParms;
    cnt += RunInChild(TestLoad, 1, parms);

    /*
     * Test F:
     */
    fprintf(stdout, "Test 11f: 10 segs, 10k size, 8k offt,  10000 cnt, 1/10/5 trunc/txn/op: ");
    fflush(stdout);
    system("rm -rf " TEST_DIR);
    loadParms.segSize = 10*1024;
    loadParms.numCountIncs=10000;
    loadParms.numSegs=10;
    loadParms.numOpsPerTxn = 5;
    loadParms.numTxnsPerTrunc = 10;
    loadParms.intOffset = 8*1024;
    parms[0] = (void *) &loadParms;
    cnt += RunInChild(TestLoad, 1, parms);

    /*
     * Test G:
     */
    fprintf(stdout, "Test 11g: 100 segs, 2k size, 1k offt,  10000 cnt, 1/10/10 trunc/txn/op: ");
    fflush(stdout);
    gettimeofday(&tstart, NULL);
    system("rm -rf " TEST_DIR);
    loadParms.segSize = 2*1024;
    loadParms.numCountIncs=10000;
    loadParms.numSegs=100;
    loadParms.numOpsPerTxn = 10;
    loadParms.numTxnsPerTrunc = 10;
    loadParms.intOffset = 1*1024;
    parms[0] = (void *) &loadParms;

    gettimeofday(&tstart, NULL);
    cnt += RunInChild(TestLoad, 1, parms);
    gettimeofday(&tend, NULL);

    /*
     * calculate elapsed time
     */
    if( tend.tv_usec < tstart.tv_usec )
    {
        tend.tv_usec += 1000000;
        tend.tv_sec--;
    }

    tend.tv_sec = tend.tv_sec - tstart.tv_sec;
    tend.tv_usec = tend.tv_usec - tstart.tv_usec;

    /*
     * Test H:
     */
    fprintf(stdout, "Test 11h: same test as 11g, but interrupted & restarted repeatedly: ");
    fflush(stdout);
    system("rm -rf " TEST_DIR);
    loadParms.segSize = 2*1024;
    loadParms.numCountIncs=10000;
    loadParms.numSegs=100;
    loadParms.numOpsPerTxn = 10;
    loadParms.numTxnsPerTrunc = 10;
    loadParms.intOffset = 1*1024;
    sleepTime = (tend.tv_sec * 1000000 + tend.tv_usec) / 99;  // allow for 99 interrupts
    parms[0] = (void *) &loadParms;
    cnt += InterruptedRunInChild(TestLoad, sleepTime, 1, parms);

    return(cnt);
}

static int
TestLoad( int parmcnt, void **parms )
{
    char              buf[512];
    int               cnt;
    int             **counters;
    int               intOffset;
    LoadDef_t       * l;
    int               op;
    int               maxCnt;
    int               maxOps;
    int               maxSegs;
    int               maxTxns;
    void            **pMems;
    int               rtn;
    rvm_t             rvm1;
    int               seg;
    int               segSize;
    trans_t           tid;
    int               txn;

    assert(parmcnt == 1);

    l = (LoadDef_t *) parms[0];

    maxCnt = l->numCountIncs * l->numSegs;
    maxSegs = l->numSegs;
    maxOps  = l->numOpsPerTxn;
    maxTxns = l->numTxnsPerTrunc;
    intOffset = l->intOffset;
    segSize = l->segSize;

    assert( intOffset < (segSize-sizeof(int)));


    /*
     * get our handle, if we can't, no tests can be run...
     */
    assert((rvm1 = rvm_init(TEST_DIR)) != NULL );

    /*
     * map the test segments
     */
    assert( (pMems = malloc(maxSegs * sizeof(*pMems))) != NULL);
    for(seg=0; seg < maxSegs; seg++)
    {
        snprintf(buf, sizeof(buf), TEST_SEG "_%d", seg);

        assert( (pMems[seg] = rvm_map(rvm1, buf, segSize)) != NULL );
    }

    /*
     * map our integer counters
     */
    assert( (counters = malloc(maxSegs * sizeof(*pMems))) != NULL);
    for(seg=0; seg < maxSegs; seg++)
    {
        counters[seg] = (int *) ( ((char *)pMems[seg])+intOffset);
    }

    /*
     * if we're picking up old data, make sure the data is valid
     */
    if( *counters[0] != 0 )
    {
        for(seg=0; seg < (maxSegs-1); seg++)
        {
            if( *counters[seg+1] != (*counters[seg]+1) )
            {
                fprintf(stdout,"ERROR: counters out of sync position %d, dumping counters:\n", seg);
                for(seg=0; seg < maxSegs; seg++)
                {
                    fprintf(stdout, "   seg[%d] counter = %d\n", seg, *counters[seg]);
                }
                exit(10);
            }
        }
        cnt = *counters[maxSegs-1]+1;
    }
    else
    {
        cnt = 0;
    }

    /*
     * start the transaction and setup modification areas
     */
    assert( (tid = rvm_begin_trans(rvm1,  maxSegs, pMems)) != (trans_t) -1);
    for(seg=0; seg < maxSegs; seg++)
    {
        rvm_about_to_modify(tid,  pMems[seg], intOffset, sizeof(int));
    }

    seg = txn = op = 0;
    while( cnt < maxCnt )
    {
        *counters[seg] = cnt++;

        if( ++seg >= maxSegs )
        {
            if( ++op >= maxOps )
            {
                rvm_commit_trans(tid);

                if( ++txn >= maxTxns )
                {
                    rvm_truncate_log(rvm1);

                    txn = 0;
                }

                assert( (tid = rvm_begin_trans(rvm1,  maxSegs, pMems)) != (trans_t) -1);
                for(seg=0; seg < maxSegs; seg++)
                {
                    rvm_about_to_modify(tid,  pMems[seg], intOffset, sizeof(int));
                }

                op = 0;
            }

            seg = 0;
        }
    }

    signal(SIGINT, SIG_IGN);    // turn off SIGNINT at this point

    rvm_commit_trans(tid);
    rvm_truncate_log(rvm1);

    /*
     * unmap and remap the test segments
     */
    for(seg=0; seg < maxSegs; seg++)
    {
        rvm_unmap(rvm1, pMems[seg]);

        snprintf(buf, sizeof(buf), TEST_SEG "_%d", seg);

        /*
         * map a simple segment  (again for testing);
         */
        assert( (pMems[seg] = rvm_map(rvm1, buf, segSize)) != NULL );
    }

    /*
     * map our integer counters
     */
    for(seg=0; seg < maxSegs; seg++)
    {
        counters[seg] = (int *) ( ((char *)pMems[seg])+intOffset);
    }

    /*
     * verify the last counter
     */
    rtn = TestSuccess( *counters[maxSegs-1] == (cnt-1));

    return(rtn);

} /* TestLoad(... */


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
        if( WIFEXITED(exitstatus) )
        {
            if((WEXITSTATUS(exitstatus) != 0) )
            {
                if( exitOnError )
                {
                    exit(10);
                }
                rtn++;
            }
        }
        else if( WIFSIGNALED(exitstatus) )
        {
            if( WTERMSIG(exitstatus) != SIGINT )
            {
                fprintf(stderr, "Child caught signal %d\n", WTERMSIG(exitstatus));
                if( exitOnError )
                {
                    exit(10);
                }
                rtn++;
            }
        }
        else
        {
            fprintf(stderr,"Child did not exit cleanly:  Exit status: %d\n", exitstatus);
            if( exitOnError )
            {
                exit(10);
            }
            rtn++;
        }
    }

    return(rtn);

} /* RunInChild(... */

static int
InterruptedRunInChild( int (*func)(), long sleepTime, int cnt, void **parms)
{
    extern int        errno;
    int               exitstatus;
    pid_t             pid;
    int               rtn = 0;

    for(;;)
    {
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
            /*
             * waite 1/10 sec then kill child
             */
            if( sleepTime > 1000000 )
            {
                sleep(sleepTime/1000000);
            }
            else
            {
                usleep(sleepTime);
            }
            kill(pid, SIGINT);

            if( waitpid(pid, &exitstatus,0) == -1 )
            {
                fprintf(stderr,"Internal error: Faild to wait on child process, errno=%d\n", errno);
                exit(20);
            }
            if( WIFEXITED(exitstatus) )
            {
                if((WEXITSTATUS(exitstatus) != 0) )
                {
                    if( exitOnError )
                    {
                        exit(10);
                    }
                    rtn++;
                }
                break;
            }
            else if( WIFSIGNALED(exitstatus) )
            {
                if( WTERMSIG(exitstatus) != SIGINT )
                {
                    fprintf(stderr, "Child caught signal %d\n", WTERMSIG(exitstatus));
                    if( exitOnError )
                    {
                        exit(10);
                    }
                    rtn++;
                    break;
                }
            }
            else
            {
                fprintf(stderr,"Child did not exit cleanly:  Exit status: %d\n", exitstatus);
                if( exitOnError )
                {
                    exit(10);
                }
                rtn++;
                break;
            }
        }
    }

    return(rtn);

} /* InterruptedRunInChild(... */

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


