struct Point { int x; int y; };
struct Point make_point(int x, int y);
void move_point(struct Point *p, int dx, int dy);
struct Missing;
void take_missing(struct Missing *m);
