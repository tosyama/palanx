#include <stdio.h>
#include <stdarg.h>
#include "296_struct_byval.h"

struct I8 i8_add(struct I8 v, int d) { printf("i8 %d %d\n", v.a, v.b); v.a += d; v.b += d; return v; }
struct I16 i16_add(struct I16 v, long d) { printf("i16 %ld %ld\n", v.a, v.b); v.a += d; v.b += d; return v; }
struct F2 f2_add(struct F2 v, float d) { printf("f2 %.1f %.1f\n", v.x, v.y); v.x += d; v.y += d; return v; }
struct D2 d2_add(struct D2 v, double d) { printf("d2 %.1f %.1f\n", v.x, v.y); v.x += d; v.y += d; return v; }
struct Mix mix_add(struct Mix v, int d) { printf("mix %.1f %d\n", v.d, v.i); v.d += d; v.i += d; return v; }
struct Mix2 mix2_add(struct Mix2 v, int d) { printf("mix2 %d %.1f\n", v.i, v.f); v.i += d; v.f += d; return v; }
struct B3 b3_add(struct B3 v, char d) { printf("b3 %d %d %d\n", v.a, v.b, v.c); v.a += d; v.c += d; return v; }
struct B5 b5_add(struct B5 v, char d)
{
	printf("b5 %d %d %d %d %d\n", v.a, v.b, v.c, v.d, v.e);
	v.a += d; v.e += d; return v;
}
struct B6 b6_add(struct B6 v, short d) { printf("b6 %d %d %d\n", v.a, v.b, v.c); v.a += d; v.c += d; return v; }
struct B7 b7_add(struct B7 v, char d)
{
	printf("b7 %d %d %d %d %d %d %d\n", v.a, v.b, v.c, v.d, v.e, v.f, v.g);
	v.a += d; v.g += d; return v;
}
struct B11 b11_add(struct B11 v, char d)
{
	printf("b11 %d %d %d\n", v.c[0], v.c[8], v.c[10]);
	v.c[0] += d; v.c[10] += d; return v;
}
struct Big big_add(struct Big v, long d) { printf("big %ld %ld %ld\n", v.a, v.b, v.c); v.a += d; v.c += d; return v; }
struct Big20 big20_add(struct Big20 v, char d)
{
	printf("big20 %d %d\n", v.c[0], v.c[19]);
	v.c[0] += d; v.c[19] += d; return v;
}
union U u_bits(union U v) { printf("u %x\n", v.i); v.i ^= 1; return v; }
void int_regs_full(long a, long b, long c, long d, long e, struct I16 s, long g)
{
	printf("int_regs_full %ld %ld %ld %ld %ld %ld %ld %ld\n", a, b, c, d, e, s.a, s.b, g);
}
void flt_regs_full(double a, double b, double c, double d, double e, double f, double g,
                   struct D2 s, double h)
{
	printf("flt_regs_full %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.1f %.1f %.0f\n",
	       a, b, c, d, e, f, g, s.x, s.y, h);
}
void variadic_after(struct Mix m, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	printf("variadic_after %.1f %d ", m.d, m.i);
	vprintf(fmt, ap);
	va_end(ap);
}
