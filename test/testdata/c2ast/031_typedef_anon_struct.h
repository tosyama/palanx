// Single, non-derived declarator: the anonymous body's tag is synthesized
// from the typedef name "Div".
typedef struct { int quot; int rem; } Div;
Div make_div(int, int);
int div_sum(Div *p);

// Multi-declarator: non-goal, stays unresolved "user".
typedef struct { int a; } Multi, *MultiPtr;
Multi take_multi(void);

// Derived (pointer) declarator: non-goal, the var-type is wrapped in "pntr"
// before reaching the synthesis guard, so the pointee stays an untagged
// "strct".
typedef struct { int b; } *DerivedPtr;
void take_derived(DerivedPtr p);

// Tag name clash: "Clash" is already a real C tag (defined here), so the
// later anonymous-body typedef of the same name is skipped rather than
// promoting Clash's entry with these unrelated fields.
struct Clash { int k; };
typedef struct { int m; } Clash;
Clash use_clash(void);
