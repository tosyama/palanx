int old_f(int a);
int new_f(int a, int b);
#define old_f new_f

#define missing_f not_declared

int kept_f(int a);
int other_f(int a);
#define kept_f other_f
#undef kept_f

int last_f(void);
#define mid_f last_f
#define first_f mid_f
