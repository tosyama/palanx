typedef enum { CS_GRAY = 1, CS_RGB, CS_CMYK = -1 } CSpace;
enum Method { M_SLOW, M_FAST };
struct Conf { CSpace cs; enum Method method; char tag; };
#define M_DEFAULT M_FAST
enum { LIMIT = 100 };
