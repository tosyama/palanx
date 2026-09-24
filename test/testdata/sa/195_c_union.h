typedef union U { char b[12]; long l; } u_t;
struct S { int k; union U u; };
union W { struct Pt { int x; int y; } p; long l; };
int take(u_t *p);
