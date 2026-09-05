struct Point { int x; int y; };
struct Rec { int id; char name[16]; long vals[4]; struct Point pts[3]; };
struct Grid2D { int cells[2][3]; };

#define N 4
struct MacroSized { int arr[N]; };

int f(char buf[32], struct Rec recs[2]);
int g(char buf[2][3]);
