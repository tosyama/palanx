#include "301_frame_c.h"

struct V2 v2_make(int x, int y) { struct V2 v = {x, y}; return v; }
struct Big big_make(long a) { struct Big b = {a, a + 1, a + 2}; return b; }
int v2_sum(struct V2 v) { return v.x + v.y; }
void v2_scale(struct V2 *v, int k) { v->x *= k; v->y *= k; }
long big_sum(const struct Big *b) { return b->a + b->b + b->c; }
