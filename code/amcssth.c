/* impl.c.amcssth: POOL CLASS AMC STRESS TEST WITH TWO THREADS
 *
 * $HopeName: MMsrc!amcssth.c(trunk.3) $
 * Copyright (C) 2001 Harlequin Limited.  All rights reserved.
 *
 * .posix: This is Posix only.
 */

#define _POSIX_C_SOURCE 199309L

#include "fmtdy.h"
#include "testlib.h"
#include "mpscamc.h"
#include "mpsavm.h"
#include "mpstd.h"
#ifdef MPS_OS_W3
#include "mpsw3.h"
#endif
#include "mps.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>


/* These values have been tuned to cause one top-generation collection. */
#define testArenaSIZE     ((size_t)1000*1024)
#define avLEN             3
#define exactRootsCOUNT   200
#define ambigRootsCOUNT   50
#define genCOUNT          2
#define collectionsCOUNT  37
#define rampSIZE          9
#define initTestFREQ      6000

/* testChain -- generation parameters for the test */

static mps_gen_param_s testChain[genCOUNT] = {
  { 150, 0.85 }, { 170, 0.45 } };


/* objNULL needs to be odd so that it's ignored in exactRoots. */
#define objNULL           ((mps_addr_t)0xDECEA5ED)


static mps_pool_t pool;
static mps_addr_t exactRoots[exactRootsCOUNT];
static mps_addr_t ambigRoots[ambigRootsCOUNT];

mps_arena_t arena;
mps_fmt_t format;
mps_chain_t chain;
mps_root_t exactRoot, ambigRoot;
unsigned long objs = 0;


static mps_addr_t make(mps_ap_t ap)
{
  size_t length = rnd() % (2*avLEN);
  size_t size = (length+2) * sizeof(mps_word_t);
  mps_addr_t p;
  mps_res_t res;

  do {
    MPS_RESERVE_BLOCK(res, p, ap, size);
    if (res)
      die(res, "MPS_RESERVE_BLOCK");
    res = dylan_init(p, size, exactRoots, exactRootsCOUNT);
    if (res)
      die(res, "dylan_init");
  } while(!mps_commit(ap, p, size));

  return p;
}


static void test_stepper(mps_addr_t object, void *p, size_t s)
{
  (*(unsigned long *)p)++;
  testlib_unused(s);
  testlib_unused(object);
}


/* init -- initialize pool and roots */

static void init(void)
{
  size_t i;

  die(dylan_fmt(&format, arena), "fmt_create");
  die(mps_chain_create(&chain, arena, genCOUNT, testChain), "chain_create");

  die(mps_pool_create(&pool, arena, mps_class_amc(), format, chain),
      "pool_create(amc)");

  for(i = 0; i < exactRootsCOUNT; ++i)
    exactRoots[i] = objNULL;
  for(i = 0; i < ambigRootsCOUNT; ++i)
    ambigRoots[i] = (mps_addr_t)rnd();

  die(mps_root_create_table_masked(&exactRoot, arena,
                                   MPS_RANK_EXACT, (mps_rm_t)0,
                                   &exactRoots[0], exactRootsCOUNT,
                                   (mps_word_t)1),
      "root_create_table(exact)");
  die(mps_root_create_table(&ambigRoot, arena,
                            MPS_RANK_AMBIG, (mps_rm_t)0,
                            &ambigRoots[0], ambigRootsCOUNT),
      "root_create_table(ambig)");
}


/* finish -- finish pool and roots */

static void finish(void)
{
  mps_root_destroy(exactRoot);
  mps_root_destroy(ambigRoot);
  mps_pool_destroy(pool);
  mps_chain_destroy(chain);
  mps_fmt_destroy(format);
}


/* churn -- create an object and install into roots */

static void churn(mps_ap_t ap)
{
  size_t i;
  size_t r;

  ++objs;
  r = (size_t)rnd();
  if (r & 1) {
    i = (r >> 1) % exactRootsCOUNT;
    if (exactRoots[i] != objNULL)
      cdie(dylan_check(exactRoots[i]), "dying root check");
    exactRoots[i] = make(ap);
    if (exactRoots[(exactRootsCOUNT-1) - i] != objNULL)
      dylan_write(exactRoots[(exactRootsCOUNT-1) - i],
                  exactRoots, exactRootsCOUNT);
  } else {
    i = (r >> 1) % ambigRootsCOUNT;
    ambigRoots[(ambigRootsCOUNT-1) - i] = make(ap);
    /* Create random interior pointers */
    ambigRoots[i] = (mps_addr_t)((char *)(ambigRoots[i/2]) + 1);
  }
}


/* test -- the body of the test */

static void *test(void *arg, size_t s)
{
  size_t i;
  mps_word_t collections, rampSwitch;
  mps_alloc_pattern_t ramp = mps_alloc_pattern_ramp();
  int ramping;
  mps_ap_t ap, busy_ap;
  mps_addr_t busy_init;

  arena = (mps_arena_t)arg;
  (void)s; /* unused */

  die(mps_ap_create(&ap, pool, MPS_RANK_EXACT), "BufferCreate");
  die(mps_ap_create(&busy_ap, pool, MPS_RANK_EXACT), "BufferCreate 2");

  /* create an ap, and leave it busy */
  die(mps_reserve(&busy_init, busy_ap, 64), "mps_reserve busy");

  collections = 0;
  rampSwitch = rampSIZE;
  mps_ap_alloc_pattern_begin(ap, ramp);
  mps_ap_alloc_pattern_begin(busy_ap, ramp);
  ramping = 1;
  while(collections < collectionsCOUNT) {
    unsigned long c;
    size_t r;

    c = mps_collections(arena);

    if (collections != c) {
      collections = c;
      printf("\nCollection %lu, %lu objects.\n",
             c, objs);
      for(r = 0; r < exactRootsCOUNT; ++r)
        cdie(exactRoots[r] == objNULL || dylan_check(exactRoots[r]),
             "all roots check");
      if (collections == collectionsCOUNT / 2) {
        unsigned long object_count = 0;
        mps_arena_park(arena);
	mps_amc_apply(pool, test_stepper, &object_count, 0);
	mps_arena_release(arena);
	printf("mps_amc_apply stepped on %lu objects.\n", object_count);
      }
      if (collections == rampSwitch) {
        rampSwitch += rampSIZE;
        if (ramping) {
          mps_ap_alloc_pattern_end(ap, ramp);
          mps_ap_alloc_pattern_end(busy_ap, ramp);
          /* kill half of the roots */
          for(i = 0; i < exactRootsCOUNT; i += 2) {
            if (exactRoots[i] != objNULL) {
              cdie(dylan_check(exactRoots[i]), "ramp kill check");
              exactRoots[i] = objNULL;
            }
          }
          /* Every other time, switch back immediately. */
          if (collections & 1) ramping = 0;
        }
        if (!ramping) {
          mps_ap_alloc_pattern_begin(ap, ramp);
          mps_ap_alloc_pattern_begin(busy_ap, ramp);
          ramping = 1;
        }
      }
    }

    churn(ap);

    if (r % initTestFREQ == 0)
      *(int*)busy_init = -1; /* check that the buffer is still there */

    if (objs % 1024 == 0) {
      putchar('.');
      fflush(stdout);
    }
  }

  (void)mps_commit(busy_ap, busy_init, 64);
  mps_ap_destroy(busy_ap);
  mps_ap_destroy(ap);

  return NULL;
}


static void *fooey2(void *arg, size_t s)
{
  mps_ap_t ap;

  (void)arg; (void)s; /* unused */
  die(mps_ap_create(&ap, pool, MPS_RANK_EXACT), "BufferCreate(fooey)");
  while(mps_collections(arena) < collectionsCOUNT) {
    churn(ap);
  }
  mps_ap_destroy(ap);
  return NULL;
}


static void *fooey(void* childIsFinishedReturn)
{
  void *r;
  mps_thr_t thread;
  mps_thr_t thread2;

  /* register the thread twice, just to make sure it works */
  die(mps_thread_reg(&thread, (mps_arena_t)arena), "thread_reg");
  die(mps_thread_reg(&thread2, (mps_arena_t)arena), "thread2_reg");
  mps_tramp(&r, fooey2, NULL, 0);
  mps_thread_dereg(thread);
  mps_thread_dereg(thread2);
  *(int *)childIsFinishedReturn = 1;
  return r;
}


int main(int argc, char **argv)
{
  mps_thr_t thread;
  pthread_t pthread1;
  void *r;
  int childIsFinished = 0;

  randomize(argc, argv);

  die(mps_arena_create(&arena, mps_arena_class_vm(), testArenaSIZE),
      "arena_create");
  init();
  die(mps_thread_reg(&thread, arena), "thread_reg");
  pthread_create(&pthread1, NULL, fooey, (void *)&childIsFinished);
  mps_tramp(&r, test, arena, 0);
  mps_thread_dereg(thread);

  while (!childIsFinished) {
    struct timespec req = {1, 0};
    (void)nanosleep(&req, NULL);
  }

  finish();
  mps_arena_destroy(arena);

  fflush(stdout); /* synchronize */
  fprintf(stderr, "\nConclusion:  Failed to find any defects.\n");
  return 0;
}