/* impl.c.apss: AP MANUAL ALLOC STRESS TEST
 *
 * $HopeName$
 * Copyright (C) 2000 Harlequin Limited.  All rights reserved.
 */


#include "mpscmv.h"
#include "mpscmvff.h"
#include "mpslib.h"
#include "mpsavm.h"

#include "testlib.h"

#include <stdlib.h>
#include <stdarg.h>


#define testArenaSIZE   ((((size_t)3)<<24) - 4)
#define testSetSIZE 200
#define testLOOPS 10


static mps_res_t make(mps_addr_t *p, mps_ap_t ap, size_t size)
{
  mps_res_t res;

  do {
    MPS_RESERVE_BLOCK(res, *p, ap, size);
    if(res != MPS_RES_OK)
      return res;
  } while(!mps_commit(ap, *p, size));

  return MPS_RES_OK;
}


static mps_res_t stress(mps_class_t class, mps_arena_t arena,
                        size_t (*size)(int i), ...)
{
  mps_res_t res = MPS_RES_OK;
  mps_pool_t pool;
  mps_ap_t ap;
  va_list arg;
  int i, k;
  int *ps[testSetSIZE];
  size_t ss[testSetSIZE];

  va_start(arg, size);
  res = mps_pool_create_v(&pool, arena, class, arg);
  va_end(arg);
  if (res != MPS_RES_OK)
    return res;

  die(mps_ap_create(&ap, pool, MPS_RANK_EXACT), "BufferCreate");

  /* allocate a load of objects */
  for (i=0; i<testSetSIZE; ++i) {
    ss[i] = (*size)(i);

    res = make((mps_addr_t *)&ps[i], ap, ss[i]);
    if (res != MPS_RES_OK)
      goto allocFail;
    if (ss[i] >= sizeof(ps[i]))
      *ps[i] = 1; /* Write something, so it gets swap. */
  }

  mps_pool_check_fenceposts(pool);

  for (k=0; k<testLOOPS; ++k) {
    /* shuffle all the objects */
    for (i=0; i<testSetSIZE; ++i) {
      int j = rand()%(testSetSIZE-i);
      void *tp;
      size_t ts;
      
      tp = ps[j]; ts = ss[j];
      ps[j] = ps[i]; ss[j] = ss[i];
      ps[i] = tp; ss[i] = ts;
    }
    /* free half of the objects */
    /* upper half, as when allocating them again we want smaller objects */
    /* see randomSize() */
    for (i=testSetSIZE/2; i<testSetSIZE; ++i) {
      mps_free(pool, (mps_addr_t)ps[i], ss[i]);
      /* if (i == testSetSIZE/2) */
      /*   PoolDescribe((Pool)pool, mps_lib_stdout); */
    }
    /* allocate some new objects */
    for (i=testSetSIZE/2; i<testSetSIZE; ++i) {
      ss[i] = (*size)(i);
      res = make((mps_addr_t *)&ps[i], ap, ss[i]);
      if (res != MPS_RES_OK)
        goto allocFail;
    }
  }

allocFail:
  mps_ap_destroy(ap);
  mps_pool_destroy(pool);

  return res;
}


#define max(a, b) (((a) > (b)) ? (a) : (b))

#define alignUp(w, a)       (((w) + (a) - 1) & ~((size_t)(a) - 1))

static size_t randomSize8(int i)
{
  size_t maxSize = 2 * 160 * 0x2000;
  /* Reduce by a factor of 2 every 10 cycles.  Total allocation about 40 MB. */
  return alignUp(rnd() % max((maxSize >> (i / 10)), 2) + 1, 8);
}


static mps_pool_debug_option_s debugOptions = { (void *)"postpost", 8 };

static void testInArena(mps_arena_t arena)
{
  mps_res_t res;

  printf("MVFF\n\n");
  res = stress(mps_class_mvff(), arena, randomSize8,
               (size_t)65536, (size_t)32, (size_t)4, TRUE, TRUE, TRUE);
  if (res == MPS_RES_COMMIT_LIMIT) return;
  die(res, "stress MVFF");
  printf("MV debug\n\n");
  res = stress(mps_class_mv_debug(), arena, randomSize8,
               &debugOptions, (size_t)65536, (size_t)32, (size_t)65536);
  if (res == MPS_RES_COMMIT_LIMIT) return;
  die(res, "stress MV debug");
  printf("MV\n\n");
  res = stress(mps_class_mv(), arena, randomSize8,
               (size_t)65536, (size_t)32, (size_t)65536);
  if (res == MPS_RES_COMMIT_LIMIT) return;
  die(res, "stress MV");
}


int main(int argc, char **argv)
{
  mps_arena_t arena;

  randomize(argc, argv);

  die(mps_arena_create(&arena, mps_arena_class_vm(), 2*testArenaSIZE),
      "mps_arena_create");
  mps_arena_commit_limit_set(arena, testArenaSIZE);
  testInArena(arena);
  mps_arena_destroy(arena);

  die(mps_arena_create(&arena, mps_arena_class_vmnz(), 2*testArenaSIZE),
      "mps_arena_create");
  testInArena(arena);
  mps_arena_destroy(arena);

  fflush(stdout); /* synchronize */
  fprintf(stderr, "\nConclusion:  Failed to find any defects.\n");
  return 0;
}