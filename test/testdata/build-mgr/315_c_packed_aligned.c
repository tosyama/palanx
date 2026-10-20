#include <stdio.h>
#include <stddef.h>
#include "315_c_packed_aligned.h"

void layout(void)
{
	printf("Pk %zu %zu %zu, Al %zu, Fa %zu %zu %zu, TPk %zu, Outer %zu %zu %zu\n",
		sizeof(struct Pk), offsetof(struct Pk, i), offsetof(struct Pk, l),
		sizeof(struct Al), sizeof(struct Fa), offsetof(struct Fa, i), offsetof(struct Fa, l),
		sizeof(TPk), sizeof(struct Outer), offsetof(struct Outer, pk), offsetof(struct Outer, two));
}

void show_pk(struct Pk v) { printf("pk %d %d %ld\n", v.c, v.i, v.l); }

struct Pk pk_add(struct Pk v, int d)
{
	v.c += d; v.i += d; v.l += d;
	return v;
}

void fill_pk(struct Pk *a, int n)
{
	for (int k = 0; k < n; k++) { a[k].c = k; a[k].i = 10 + k; a[k].l = 100 + k; }
}

void fill_al(struct Al *a, int n)
{
	for (int k = 0; k < n; k++) a[k].x = 20 + k;
}

void show_fa(struct Fa *p) { printf("fa %d %d %ld\n", p->c, p->i, p->l); }

void show_tpk(TPk v) { printf("tpk %d %d\n", v.c, v.s); }

void show_outer(struct Outer *o)
{
	printf("outer %d %d %d %ld %ld\n", o->c, o->pk.c, o->pk.i, o->pk.l, o->two[1].l);
}
