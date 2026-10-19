struct V2 { int x; int y; };
struct Big { long a, b, c; };
struct V2 v2_make(int x, int y);
struct Big big_make(long a);
int v2_sum(struct V2 v);
void v2_scale(struct V2 *v, int k);
long big_sum(const struct Big *b);
