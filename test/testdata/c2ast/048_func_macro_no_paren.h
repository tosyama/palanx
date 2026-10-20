#define F(x) (x+1)
#define G F
#define H(x) x
#define K F
int testproc(){return F;return F + 1;return H(F);return G(2);return H(F)(3);}
