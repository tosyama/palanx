#pragma once
#define A_FLAG
#define A_VAL 3
#define A_GONE 9
#define A_CAT(x, y) x ## y
#define A_STR(x) #x
#define A_VARGS(...) f(__VA_ARGS__)
#define A_SHIFT (1 << 2)
#define A_TEXT "a b"
#define A_CH 'c'
#undef __CHAR_BIT__
int a_func(void);
