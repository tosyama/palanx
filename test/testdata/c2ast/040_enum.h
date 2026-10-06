enum Color { RED, GREEN = 5, BLUE, NEG = -3, AFTER = BLUE + 1 };
typedef enum { CS_A, CS_B } CSpace;
typedef enum Mode { MODE_X = 1 << 2 } ModeAlias;
enum { ANON_A = 10, ANON_B };
enum Unknown { U1 = sizeof(int), U2 };
struct S { enum Color c; CSpace cs; int a[AFTER]; enum { IN_A } in; };
int f(enum Color c, ModeAlias m);
extern CSpace g_cs;
#define DEF_COLOR GREEN
#define NEXT_COLOR (BLUE + 1)
#define ANON_A ANON_A
#define MODE_X MODE_X
#define UNKNOWN_M U1
enum Color get_color(void);
