struct S { int x; };
union U { int x; float y; };
enum E { E_A, E_B };

// Pointee const on struct/union/enum-typed pointers -- previously dropped
// because declaration_specifiers' struct/union/enum branches ignored is_const.
int f_struct(const struct S *s);
int f_union(const union U *u);
int f_enum(const enum E *e);

// Struct fields: qualifiers must not be pre-consumed and discarded before
// declaration_specifiers gets a chance to capture "const". Also cover
// "volatile const" (order swapped from the usual "const volatile") to
// confirm the qualifier loop in declaration_specifiers is order-independent.
struct Rec {
	const int a;
	volatile const int b;
	int c;
};
