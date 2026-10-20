extern int (*hook)(int);
struct H { int (*cb)(int); int n; };
struct Inc;
struct Inc* getinc(void);
