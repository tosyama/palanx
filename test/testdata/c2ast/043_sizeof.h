typedef long word;
typedef word *word_p;
struct Pad { char c; double d; short s; };
union U { char c[5]; int i; };
struct Nest { char c; struct Pad p; union U u; struct { char x; int y; } anon; };
struct Flex { short n; long data[]; };
enum Small { S1 = -1, S2 = 0x7fffffff };
enum Big { B1 = 0x100000000 };
struct Opaque;
struct Arrs { char a[sizeof(int) * 3]; struct Pad p[2]; };

#define SZ_CHAR    sizeof(char)
#define SZ_SHORT   sizeof(unsigned short)
#define SZ_INT     sizeof(int)
#define SZ_LONG    sizeof(long int)
#define SZ_DOUBLE  sizeof(double)
#define SZ_LDOUBLE sizeof(long double)
#define SZ_BOOL    sizeof(_Bool)
#define SZ_PTR     sizeof(void *)
#define SZ_FUNCPTR sizeof(int (*)(int))
#define SZ_TYPEDEF sizeof(word)
#define SZ_TDPTR   sizeof(word_p)
#define SZ_ARR     sizeof(int[3][2])
#define SZ_PAD     sizeof(struct Pad)
#define SZ_UNION   sizeof(union U)
#define SZ_NEST    sizeof(struct Nest)
#define SZ_FLEX    sizeof(struct Flex)
#define SZ_SMALL   sizeof(enum Small)
#define SZ_BIG     sizeof(enum Big)
#define SZ_ARRS    sizeof(struct Arrs)
#define SZ_CAST    (8 * (int) sizeof(word))
#define SZ_EXPR    (15 * sizeof(int) - 4 * sizeof(void *) - sizeof(long))
#define SZ_VOID     sizeof(void)           /* skipped: GNU extension */
#define SZ_OPAQUE   sizeof(struct Opaque)  /* skipped: incomplete */
#define SZ_UNKNOWN  sizeof(unknown_t)      /* skipped: unresolved typedef name */
