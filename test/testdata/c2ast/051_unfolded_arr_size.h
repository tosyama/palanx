struct T { int n; char tail[1 ? 4 : 8]; };
struct V { int n; char tail[N_UNDEF]; };
struct U { int n; char tail[]; };
#define SZ_T sizeof(struct T)
#define SZ_V sizeof(struct V)
#define SZ_U sizeof(struct U)
