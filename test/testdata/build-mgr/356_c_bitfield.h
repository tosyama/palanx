struct Bf { char c; unsigned int a:4, b:6; int :0; char d; short e:3; int f; };
struct __attribute__((packed)) Bp { char c; int a:20; int b:20; char d; };
struct Bl { long long a:40; int b:20; char c; };
struct Bs { float x; int a:8; float y; };

void layout(void);
void set_bf(struct Bf *p);
void show_bf(const struct Bf *p);
void show_bp(struct Bp v);
void show_bl(const struct Bl *p);
void show_bs(struct Bs v);
struct Bs make_bs(void);
