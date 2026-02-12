#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <locale.h>
#include <ncurses.h>
#include <jpeglib.h>
#include <sqlite3.h>
#include <pthread.h>
#include <unistd.h>

#define F(x) CHECK(G(x))
#define G(x) CHECK(H(x))
#define CHECK(x) x

#define A (A+F(1))

//#define F(x) x
//#define G F


#if defined(Z) || defined(A)
#define S 1
#else
#define S 0
#endif

#define PASTE(x) x ## y ## z

int testproc() {
	return PASTE(S);
}

