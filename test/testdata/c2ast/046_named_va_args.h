#define LOG(fmt, args...) mylog(fmt, args)
#define STR(...) #__VA_ARGS__
#define NSTR(xs...) #xs
#define GROUP(TAG, MEMBERS...) union { struct { MEMBERS } a; struct TAG { MEMBERS } b; }

int testproc() {
	LOG("none");
	LOG("one", 1);
	LOG("many", 1, (2, 3), 4);
	STR(a, b);
	NSTR(c, d);
}
GROUP(tg, int x; int y;) g;
