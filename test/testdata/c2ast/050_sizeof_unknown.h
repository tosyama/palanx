struct Inc;
struct A2 { int a[-1]; };
struct A3 { struct Inc x[2]; };
struct A4 { long a[2305843009213693952]; };
struct A5 { int n; struct Inc tail[]; };
enum E2 { X = not_constant };
enum E4 { NEG = -2147483649 };
#define SZ_A2 sizeof(struct A2)
#define SZ_A3 sizeof(struct A3)
#define SZ_A4 sizeof(struct A4)
#define SZ_A5 sizeof(struct A5)
#define SZ_E2 sizeof(enum E2)
#define SZ_E4 sizeof(enum E4)
#define SZ_EU sizeof(enum Undeclared)
#define BN ~not_constant
