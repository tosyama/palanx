typedef enum {
    COLOR_RED = 0,
    COLOR_GREEN,
    COLOR_BLUE
} Color;

typedef struct {
    int x;
    int y;
} Point;

Color get_color(void);
Point make_point(int x, int y);
int point_sum(Point p);
