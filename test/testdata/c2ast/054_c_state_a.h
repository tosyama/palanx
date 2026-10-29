#pragma once
typedef unsigned short st_u16;
struct st_later;
struct st_t { int x; };
enum st_e { ST_E0, ST_E1, ST_E2 };
enum { ST_ANON = 3 };
typedef struct { char c; } st_anon;
struct st_t st_get(struct st_later *p);
