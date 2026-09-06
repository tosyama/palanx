struct PtrArr {
	int *ptrs[3];
	int (*grouped)[3];
	void (*fp)(int);
};

int f(char *argv[]);

struct QualPtr {
	int * const * p;
};
