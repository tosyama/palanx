struct Opaque;
enum Color { RED = sizeof(struct Opaque), GREEN };
void pick(enum Color c);
