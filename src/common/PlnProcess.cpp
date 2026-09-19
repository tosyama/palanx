/// Child process launcher.
///
/// @file		PlnProcess.cpp
/// @copyright	2026 YAMAGUCHI Toshinobu

#include "PlnProcess.h"

#include <spawn.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cerrno>
#include <boost/assert.hpp>

extern char **environ;

using std::string;
using std::vector;

namespace {

vector<char*> toCArgv(const vector<string>& argv)
{
	vector<char*> cargv;
	cargv.reserve(argv.size() + 1);
	for (const string& a : argv)
		cargv.push_back(const_cast<char*>(a.c_str()));
	cargv.push_back(nullptr);
	return cargv;
}

PlnProcResult waitForChild(pid_t pid)
{
	int wstatus = 0;
	for (;;) {
		if (waitpid(pid, &wstatus, 0) >= 0) break;
		if (errno == EINTR) continue;
		return { PlnSpawnStatus::SpawnFailed, 0, 0, errno };	// LCOV_EXCL_LINE
	}

	if (WIFEXITED(wstatus))
		return { PlnSpawnStatus::Exited, WEXITSTATUS(wstatus), 0, 0 };
	if (WIFSIGNALED(wstatus))
		return { PlnSpawnStatus::Signaled, 0, WTERMSIG(wstatus), 0 };
	return { PlnSpawnStatus::SpawnFailed, 0, 0, 0 };	// LCOV_EXCL_LINE (stopped/continued; not requested)
}

} // namespace

PlnProcResult runProcess(const vector<string>& argv)
{
	BOOST_ASSERT(!argv.empty());
	vector<char*> cargv = toCArgv(argv);

	pid_t pid;
	int rc = posix_spawnp(&pid, cargv[0], nullptr, nullptr, cargv.data(), environ);
	if (rc != 0)
		return { PlnSpawnStatus::SpawnFailed, 0, 0, rc };

	return waitForChild(pid);
}

PlnProcResult runProcessCaptureOut(const vector<string>& argv, string& out)
{
	BOOST_ASSERT(!argv.empty());
	out.clear();
	vector<char*> cargv = toCArgv(argv);

	int fds[2];
	if (pipe(fds) != 0)
		return { PlnSpawnStatus::SpawnFailed, 0, 0, errno };	// LCOV_EXCL_LINE

	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_adddup2(&fa, fds[1], STDOUT_FILENO);
	posix_spawn_file_actions_addclose(&fa, fds[1]);
	posix_spawn_file_actions_addclose(&fa, fds[0]);

	pid_t pid;
	int rc = posix_spawnp(&pid, cargv[0], &fa, nullptr, cargv.data(), environ);
	posix_spawn_file_actions_destroy(&fa);

	if (rc != 0) {
		close(fds[0]);	// LCOV_EXCL_START
		close(fds[1]);
		return { PlnSpawnStatus::SpawnFailed, 0, 0, rc };
	}	// LCOV_EXCL_STOP

	// The write end must be closed in the parent before reading, or read()
	// never sees EOF (the child's copy alone does not close the pipe).
	close(fds[1]);

	char buf[4096];
	ssize_t n;
	while ((n = read(fds[0], buf, sizeof(buf))) != 0) {
		if (n > 0) {
			out.append(buf, n);
		} else if (errno != EINTR) {
			break;	// LCOV_EXCL_LINE
		}
	}
	close(fds[0]);

	// Draining the pipe to EOF before reaping avoids deadlocking on output
	// larger than the pipe buffer (64KiB) -- a real C header's AST easily
	// exceeds that.
	return waitForChild(pid);
}
