typedef unsigned int chtype;
#define NCURSES_CAST(type,value) (type)(value)
#define NCURSES_ATTR_SHIFT       8
#define NCURSES_BITS(mask,shift) (NCURSES_CAST(chtype,(mask)) << ((shift) + NCURSES_ATTR_SHIFT))
#define A_REVERSE NCURSES_BITS(1U,10)
#define A_BOLD    NCURSES_BITS(1U,13)
#define ULMAX     18446744073709551615UL
