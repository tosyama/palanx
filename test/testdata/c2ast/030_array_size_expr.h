struct Sizes {
	char a[100 - 10 - 3];        /* size-expr lit-int "87" */
	char b[4 * sizeof(int) - 2]; /* size-expr null: sizeof isn't evaluated */
};
int g(void);
