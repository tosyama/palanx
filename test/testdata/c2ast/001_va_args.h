#define LOG(fmt, ...) mylog(fmt, __VA_ARGS__)

int testproc() {
	LOG("hello", 1, 2);
}
