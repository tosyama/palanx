#ifdef USE_BIG
#define LIMIT 100
#else
#define LIMIT 1
#endif
#ifdef USE_PUTS
int puts(const char *s);
#endif
