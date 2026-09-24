typedef union Attr { char size[56]; long align; } attr_t;
int attr_init(attr_t *a);

union Fwd;
int use_fwd(union Fwd *f);
union Fwd { int x; };

struct Holder { int kind; union Attr u; };
