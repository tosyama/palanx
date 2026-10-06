enum Color { RED, GREEN = 5, BLUE };
typedef enum { CS_A, CS_B } CSpace;
typedef enum Mode { MODE_X = 4 } ModeAlias;
enum { ANON_A = 10 };
struct S { enum Color c; CSpace cs; ModeAlias m[2]; };
CSpace cs_of(enum Color c, ModeAlias m);
enum Color get_color(void);
extern CSpace g_cs;
#define DEF_COLOR GREEN
