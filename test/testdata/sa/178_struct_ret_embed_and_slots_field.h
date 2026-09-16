struct Point { int x; int y; };
struct Combo { struct Point origin; void *slots[1]; };
struct Combo make_combo(void);
