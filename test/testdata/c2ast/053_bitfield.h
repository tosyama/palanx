#define WIDTH 6
enum { EWIDTH = 3 };
struct B {
	char c;
	unsigned int a:4, b:WIDTH;
	int :0;
	int :EWIDTH;
	char p:2 __attribute__((packed));
	char d;
};
union U { int a:3; short b:9; };
struct N { int x; int a:UNKNOWN; };

#define SZ_B sizeof(struct B)
#define SZ_U sizeof(union U)
#define SZ_N sizeof(struct N)
