// What sys/cdefs.h does for a compiler without __GNUC__.
#define __attribute__(xyz)
#if defined(__attribute__)
#define ATTR_MACRO_DEFINED 1
#endif

struct P { char c; int i; long l; } __attribute__((packed));
struct __attribute__((__aligned__(32))) A { int x; };
struct F { char c; int i __attribute__((aligned(8))); short s __attribute__((packed)); };
typedef struct { char c; long l; } __attribute__((packed)) TP;
typedef struct { char c; } TA __attribute__((aligned));
typedef struct __attribute__((aligned)) { char c; } TB;
struct S { __attribute__((aligned(sizeof(long)))) char c; char * __attribute__((unused)) p; };
union __attribute__((packed)) U { char c; int i; };

typedef unsigned int u16m __attribute__((__mode__(__HI__)));
typedef float v4f __attribute__((vector_size(16)));
typedef int ai8 __attribute__((aligned(8)));
typedef int aibad __attribute__((aligned(3)));
typedef int both __attribute__((mode(SI), vector_size(16)));

extern void die(const char *fmt, ...) __attribute__((__noreturn__)) __attribute__((format(printf, 1, 2), nonnull(1), ));
__attribute__((const)) int pure_f(int x __attribute__((unused)));
extern int aligned_var __attribute__((aligned(16)));
void cleanup_f(void) __attribute__(());

#define SZ_P sizeof(struct P)
#define SZ_A sizeof(struct A)
#define SZ_F sizeof(struct F)
#define SZ_TP sizeof(TP)
#define SZ_TA sizeof(TA)
#define SZ_TB sizeof(TB)
#define SZ_S sizeof(struct S)
#define SZ_U sizeof(union U)
#define AL_A _Alignof(struct A)
#define AL_P __alignof__(struct P)
