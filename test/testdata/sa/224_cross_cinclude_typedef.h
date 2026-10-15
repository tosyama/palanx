typedef my_size count_t;
struct Box { my_size n; count_t c; Kind k; };
count_t box_len(const RecT *r, my_size n, Kind k);
extern const my_size *g_size;
#define SZ ((my_size)8)
