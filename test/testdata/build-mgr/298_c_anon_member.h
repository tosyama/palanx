struct S {
    char k;
    union {
        int a;
        double d;
        struct { short lo; short hi; };
    };
    char t;
};
struct V { int tag; union { float f; int i; }; double w; };
struct F { float x; union { float y; float z; }; };

void fill(struct S *s);
void show(const struct S *s);
struct V v_add(struct V v, int d);
struct F f_add(struct F v, float d);
