#include <stdio.h>
#include "298_c_anon_member.h"

void fill(struct S *s) { s->k = 1; s->lo = 2; s->hi = 3; s->t = 4; }
void show(const struct S *s) { printf("%d %.1f %d\n", s->k, s->d, s->t); }
struct V v_add(struct V v, int d)
{
	printf("v %d %d %.1f\n", v.tag, v.i, v.w);
	v.tag += d; v.i += d; v.w += d; return v;
}
struct F f_add(struct F v, float d)
{
	printf("f %.1f %.1f\n", v.x, v.z);
	v.x += d; v.y += d; return v;
}
