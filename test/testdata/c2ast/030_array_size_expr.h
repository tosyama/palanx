struct Sizes {
	char a[100 - 10 - 3];        /* size-expr lit-int "87" */
	char b[4 * sizeof(int) - 2]; /* size-expr lit-int "14", typed size_t */
};
int g(void);
