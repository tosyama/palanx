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
#define BIGSH  (1U << 32)                    /* skipped: count >= width */
#define SOVF   ((int)0x7fffffff + (int)1)    /* skipped: signed int overflow */
#define BIGDEC 9223372036854775808L          /* skipped: no type holds it */
#define FLO    1.5f                          /* skipped: float literal */
