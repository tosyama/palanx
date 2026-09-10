#define BITS   (0400 | 0200 | 0100)   /* 448 */
#define SUB    (100 - 10 - 3)         /* 87, not 93  -- left associative */
#define SHIFTS (64 >> 2 >> 1)         /* 8,  not 64 */
#define DIVS   (100 / 5 / 2)          /* 10, not 40 */
#define MIXED  (2 + 3 * 4 - 1)        /* 13 -- precedence */
#define MASK   ((0xf0 & 0x3c) ^ 0x0f) /* 63 */
#define NEG    (-8 / 3)               /* -2 */
#define CMP    (3 > 2)                /* skipped: relational not folded */
#define DIVZERO  (1 / 0)              /* skipped */
#define BIGSHIFT (1 << 64)            /* skipped */
#define OVERFLOWED (0x7fffffffffffffff * 2)  /* skipped */
#define SIZED  (4 * sizeof(int))      /* skipped: sizeof stays null */
#define IDENT  (UNKNOWN_NAME + 1)     /* skipped: identifier stays null */
#define SUFFIXED (1L + 1)             /* skipped: suffixed literal is null */
int f(void);
