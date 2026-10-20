#include "045_macro_state_a.h"
#ifdef A_FLAG
int a_flag_on(void);
#else
int a_flag_off(void);
#endif
#ifdef __CHAR_BIT__
int char_bit_defined(void);
#endif
int A_CAT(cat, ed)(void);
int A_NVARGS(nvarg, int, char);
#define B_SHIFT A_SHIFT
#define A_VAL 5
#undef A_GONE
