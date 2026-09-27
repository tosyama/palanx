typedef unsigned int chtype;
#define U1     1U
#define UL5    5UL
#define ULL5   5ull
#define L5     5L
#define H80    0x80000000U
#define ULMAX  18446744073709551615UL
#define SH     (1U << 18)
#define AREV   ((chtype)(1U) << ((10) + 8))  /* ncurses A_REVERSE */
#define MASK   ((1U << 8) - 1U)
#define WRAP   (1U - 2U)                     /* unsigned wraps: folds */
#define NEGU   (-1U)
#define MIXNEG (-1 + 1U)                     /* int converted to unsigned int */
#define UCH    ((unsigned char)300)
#define LADD   (1L + 1)
#define UOR    (0xf0U | 0x0f)
#define UAND   (0xf0U & 0x3c)
#define UXOR   (0xffU ^ 0x0f)
#define UMUL   (3U * 5)
#define UDIV   (7U / 2)
#define UMOD   (7U % 3)
#define LU5    5LU
#define HEXL   0xFFFFFFFFFFFFFFFFL           /* hex: unsigned long when long can't hold it */
#define SNARROW ((short)40000)               /* wraps to a negative short */
#define BIGSH  (1U << 32)                    /* skipped: count >= width */
#define SOVF   ((int)0x7fffffff + (int)1)    /* skipped: signed int overflow */
#define BIGDEC 9223372036854775808L          /* skipped: no type holds it */
#define FLO    1.5f                          /* skipped: float literal */
#define UCMP   (1U < 2)                      /* skipped: relational not folded */
#define UDIVZ  (1U / 0)                      /* skipped */
#define UMODZ  (1U % 0)                      /* skipped */
#define TOOBIG 99999999999999999999U         /* skipped: exceeds 64 bits */
#define UNTBIG 0xFFFFFFFFFFFFFFFF            /* skipped: an untyped literal is limited to int64 */
#define SSHNEG ((int)-1 << 1)                /* skipped: negative signed left operand */
#define LSHOVF (1L << 63)                    /* skipped: signed shift overflow */
