struct S {
    char k;
    union {
        int a;
        double d;
        struct { short lo; short hi; };
    };
    char t;
};
