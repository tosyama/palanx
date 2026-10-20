struct S {
    int k;
    union {
        int a;
        float b;
        struct { short lo; short hi; };
    };
};

typedef union {
    long all;
    struct { int x; int y; };
} pair_t;
