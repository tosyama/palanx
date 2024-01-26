#include "testBase.h"

int cleanTestEnv()
{
	return system("rm -f -r out/*");
}

string exec_worker(const string &cmd)
{
}

string execTestCommand(const string& cmd)
{
	return "OK";
}
