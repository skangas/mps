/* impl.h.misc: MISCELLANEOUS DEFINITIONS
 *
 * $HopeName: MMsrc!misc.h(trunk.26) $
 * Copyright (C) 1999 Harlequin Limited.  All rights reserved.
 *
 * Small general things which are useful for C but aren't part of the
 * memory manager itself.  The only reason that this file exists is
 * that these things are too small and trivial to be put in their own
 * headers.  If they ever become non-trivial they should be moved out.
 */

#ifndef misc_h
#define misc_h

#include <stddef.h>


typedef int Bool;                       /* design.mps.type.bool */
enum {
  FALSE = 0,
  TRUE = 1
};


/* offsetof -- offset of field within structure
 *
 * .hack.offsetof: On platform.sus8lc the offsetof macro is not defined
 * (because LCC does not bother fixing up SunOS's broken header files).
 * We define it here using normal C constructs.  This hack is only
 * required on platform.sus8lc and no other platforms.  See
 * change.mps.tracer2.170226
 */

#ifdef MPS_PF_SUS8LC
#ifdef offsetof
#error "offsetof was unexpectedly already defined on platform SUS8LC"
#else
#define offsetof(type, field) ((size_t)(((char *)&((type *)0)->field) \
			      - (char *)0))
#endif /* offsetof */
#endif /* MPS_PF_SUS8LC */


/* SrcId -- source identification
 *
 * Every C source file should start with a SRCID declaration to
 * create a local static source identification structure.  This
 * is used by other macros (particularly assertions) and can be
 * used to reverse engineer binary deliverables.
 */

typedef const struct SrcIdStruct *SrcId;
typedef const struct SrcIdStruct {
  const char *file;
  const char *hopename;
  const char *build_date;
  const char *build_time;
} SrcIdStruct;

#define SRCID(id, hopename) \
  static SrcIdStruct FileSrcIdStruct = \
  {__FILE__, (hopename), __DATE__, __TIME__}; \
  SrcId id ## SrcId = &FileSrcIdStruct


/* BEGIN and END -- statement brackets
 *
 * BEGIN and END can be used to bracket multi-statement blocks which
 * will be followed by a semicolon, such as multi-statement macros.
 * BEGIN and END should be used to bracket ALL multi-statement macros.
 * The block, with its semicolon, still counts as a single statement.
 * This ensures that such macros can be used in all statement contexts,
 * including in the first branch of an if() statement which has an else
 * clause.
 */

#define BEGIN           do {
#define END             } while(0)



/* RVALUE -- for method-style macros
 *
 * RVALUE is used to enclose the expansion of a macro that must not be
 * used as an lvalue, e.g. a getter method.
 */

#define RVALUE(expr) ((void)0, (expr))

/* NOOP -- null statement
 *
 * Do not be tempted to use NULL, or just semicolon as the null
 * statement.  These items are dangerously ambigous and could cause
 * subtle bugs if misplaced.  NOOP is a macro which is guaranteed to
 * cause an error if it is not used in a statement context.
 */

#define NOOP   do {} while(0)


/* STR -- expands into a string of the expansion of the argument
 *
 * E.g., if we have:
 *   #define a b
 * STR(a) will expand into "b".
 */

#define STR_(x) #x
#define STR(x) STR_(x)


/* DISCARD -- discards an expression, but checks syntax
 *
 * The argument is an expression; the expansion followed by a semicolon
 * is syntactically a statement (to avoid it being used in computation).
 *
 * .discard: DISCARD uses sizeof so that the expression is not evaluated
 * and yet the compiler will check that it is a valid expression. The 
 * conditional is compared with zero so it can designate a bitfield object.
 */

#define DISCARD(expr) \
  BEGIN \
    (void)sizeof((expr)!=0); \
  END


/* DISCARD_STAT -- discards a statement, but checks syntax
 *
 * The argument is a statement; the expansion followed by a semicolon
 * is syntactically a statement.
 */

#define DISCARD_STAT(stat) \
  BEGIN \
    if (0) stat; \
  END


/* UNUSED -- declare parameter unused
 *
 * This macro supresses warnings about unused parameters.  It should be
 * applied to the parameter at the beginning of the body of the 
 * procedure.
 *
 * The cast to void appears to work for GCC, MSVC, and CodeWarrior.
 * It's a shame there's no way to ensure that the parameter won't be
 * used.  We could scramble it, but that's undesirable in release 
 * versions.
 */

#define UNUSED(param)   ((void)param)


/* PARENT -- parent structure
 *
 * Given a pointer to a field of a structure this returns a pointer to
 * the main structure.  PARENT(foo_t, x, foo->x) == foo.
 *
 * This macro is thread-safe.  design.mps.misc.parent.thread-safe
 */

#define PARENT(type, field, p) \
  ((type *)((char *)(p) - offsetof(type, field)))


/* Bit Sets -- sets of integers in [0,N-1].
 *
 * Can be used on any unsigned integral type, ty.  These definitions
 * are _syntactic_, hence macroid, hence upper case
 * (guide.c.naming.macro.special).
 */

#define BS_EMPTY(ty)            ((ty)0)
#define BS_COMP(s)              (~(s))
#define BS_UNIV(ty)             BS_COMP(BS_EMPTY(ty))
#define BS_SINGLE(ty, i)        ((ty)1 << (i))
#define BS_IS_MEMBER(s, i)      (((s) >> (i)) & 1)
#define BS_UNION(s1, s2)        ((s1) | (s2))
#define BS_ADD(ty, s, i)        BS_UNION((s), BS_SINGLE(ty, (i)))
#define BS_INTER(s1, s2)        ((s1) & (s2))
#define BS_DIFF(s1, s2)         BS_INTER((s1), BS_COMP(s2))
#define BS_DEL(ty, s, i)        BS_DIFF((s), BS_SINGLE(ty, (i)))
#define BS_SUPER(s1, s2)        (BS_INTER((s1), (s2)) == (s2))
#define BS_SUB(s1, s2)          BS_SUPER((s2), (s1))
#define BS_IS_SINGLE(s)         (((s) & ((s)-1)) == 0)
#define BS_SYM_DIFF(s1, s2)     ((s1) ^ (s2))


#endif /* misc_h */