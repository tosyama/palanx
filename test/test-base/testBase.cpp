#include "testBase.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <future>
#include <csignal>
#include <sys/stat.h>

#ifdef __GNUC__
	#include <ext/stdio_sync_filebuf.h>    
		typedef __gnu_cxx::stdio_sync_filebuf<char> popen_filebuf;
#endif
using namespace std;

int cleanTestEnv()
{
	return system("rm -f -r out/*");
}

string exec_worker(const string &cmd)
{
	system("rm -f .err");
	FILE* p = popen((cmd + " 2>.err").c_str(), "r");
	if (!p) return "exec err:" + cmd;

	popen_filebuf p_buf(p);
	istream is(&p_buf);

	string result_str;
	string tmp_str;
	getline(is, result_str);
	while (getline(is, tmp_str)) {
		result_str += tmp_str + "\n";
	}
	int ret = pclose(p);

	if (ret) {
		string err_str;
		struct stat st;
		if (stat(".err", &st) == 0) {
			ifstream errfile(".err");
			stringstream ss;
			ss << errfile.rdbuf();
			err_str = ss.str();
		}
		return "return" + to_string(WIFEXITED(ret)) + ":" + err_str + ":" +  result_str;
	}
	
	
	return result_str;
}

string execTestCommand(const string& cmd)
{
	string result_str;

	auto f = async(launch::async, exec_worker, cmd);
	auto result = f.wait_for(chrono::seconds(1));

	if (result == future_status::timeout) {
		// time out and try killing process.
		char pid_str[10];
		string pid_cmd = "pidof -s " + cmd;
		FILE *outf = popen(pid_cmd.c_str(), "r");
		fgets(pid_str, 10, outf);
		pclose(outf);
		if (pid_str[0]) {
			pid_t pid = strtoul(pid_str, NULL, 10);
			kill(pid, SIGKILL);
		}
		return "killed by timeout(1sec):" + cmd;
	}

	return f.get();
}
