struct Pk { char c; int i; long l; } __attribute__((packed));
struct __attribute__((__aligned__(32))) Al { int x; };
struct Fa { char c; int i __attribute__((aligned(8))); long l __attribute__((packed)); };
typedef struct { char c; short s; } __attribute__((packed)) TPk;
struct Outer { char c; struct Pk pk; struct Pk two[2]; };

void layout(void);
void show_pk(struct Pk v);
struct Pk pk_add(struct Pk v, int d);
void fill_pk(struct Pk *a, int n);
void fill_al(struct Al *a, int n);
void show_fa(struct Fa *p);
void show_tpk(TPk v);
void show_outer(struct Outer *o);
