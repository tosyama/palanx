#define NUM_SUFFIX(x) x ## 1
#define NUM_PREFIX(x) 1 ## x
#define NUM_NUM(a, b) a ## b
#define KW_SUFFIX(k) k ## _t

int testproc() {
	return NUM_SUFFIX(val);
	return NUM_PREFIX(val);
	return NUM_NUM(1, 2);
	return KW_SUFFIX(int);
}
