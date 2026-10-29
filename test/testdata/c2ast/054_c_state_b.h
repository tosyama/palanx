#include "054_c_state_a.h"
struct st_u { unsigned char z[sizeof(st_u16) + sizeof(struct st_t) + ST_E2 + ST_ANON]; };
struct st_later { st_u16 y; };
