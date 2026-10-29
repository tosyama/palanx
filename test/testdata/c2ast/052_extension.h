__extension__ typedef long long ll_t;
struct E {
	int a;
	__extension__ long long b;
	__extension__ union { int x; long y; };
};
ll_t f(struct E *e);
