struct Pair1 { short a; short b; };
struct Mixed { struct Pair1 items[2]; short more[4]; };
struct Mixed make_mixed(void);
