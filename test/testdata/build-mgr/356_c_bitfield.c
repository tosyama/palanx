#include <stdio.h>
#include <stddef.h>
#include "356_c_bitfield.h"

void layout(void)
{
	printf("Bf %zu %zu %zu, Bp %zu %zu, Bl %zu %zu, Bs %zu %zu\n",
		sizeof(struct Bf), offsetof(struct Bf, d), offsetof(struct Bf, f),
		sizeof(struct Bp), offsetof(struct Bp, d),
		sizeof(struct Bl), offsetof(struct Bl, c),
		sizeof(struct Bs), offsetof(struct Bs, y));
}

void set_bf(struct Bf *p) { p->a = 9; p->b = 33; p->e = -2; }

void show_bf(const struct Bf *p)
{
	printf("bf %d %u %u %d %d %d\n", p->c, p->a, p->b, p->d, p->e, p->f);
}

void show_bp(struct Bp v) { printf("bp %d %d %d %d\n", v.c, v.a, v.b, v.d); }

void show_bl(const struct Bl *p) { printf("bl %lld %d %d\n", (long long)p->a, p->b, p->c); }

void show_bs(struct Bs v) { printf("bs %.2f %d %.2f\n", v.x, v.a, v.y); }

struct Bs make_bs(void) { return (struct Bs){1.25f, 7, 3.5f}; }
