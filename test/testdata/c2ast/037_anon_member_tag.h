struct Outer { int k; struct { int a; int b; } in; };
typedef union { unsigned long long v64; struct { unsigned int lo; unsigned int hi; } v32; } wide_t;
struct Derived { struct { int x; } *p, arr[2]; };
#define PAIR(T) struct { T a; T b; }
struct Pairs { PAIR(int) ip; PAIR(double) dp; };
