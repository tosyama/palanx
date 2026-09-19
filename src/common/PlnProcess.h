/// Child process launcher.
///
/// Launches a child process from an explicit argument vector, never through
/// a shell, so no argument is ever subject to shell word-splitting, globbing
/// or command substitution.
///
/// @file		PlnProcess.h
/// @copyright	2026 YAMAGUCHI Toshinobu

#pragma once

#include <string>
#include <vector>

/// How a child process run ended.
enum class PlnSpawnStatus {
	Exited,		// The child ran to completion. exit_code is its exit status.
	Signaled,	// The child was terminated by a signal. sig_no says which.
	SpawnFailed,	// No usable exit status was produced (the child could not
			// be started, or could not be reaped). err_no says why.
};

/// Outcome of one child process run.
struct PlnProcResult {
	PlnSpawnStatus status = PlnSpawnStatus::SpawnFailed;
	int exit_code = 0;	// valid only when status == Exited
	int sig_no = 0;		// valid only when status == Signaled
	int err_no = 0;		// valid only when status == SpawnFailed (an errno value)
};

/// Run argv[0] with argv[1..] as its arguments and wait for it.
///
/// argv[0] is resolved through PATH when it contains no '/', and used as a
/// path otherwise (posix_spawnp semantics) -- so "as"/"ld" keep the PATH
/// lookup the shell used to do, and "/abs/palan-sa" or "./a.out" do not.
/// stdin, stdout and stderr are inherited unchanged, as are all other open
/// descriptors. argv must not be empty.
PlnProcResult runProcess(const std::vector<std::string>& argv);

/// As runProcess, but the child's stdout is captured into `out` rather than
/// inherited. stdin and stderr stay inherited, so the child's diagnostics
/// still reach the caller's stderr. `out` is cleared on entry and is filled
/// with whatever the child wrote regardless of how the child ended.
PlnProcResult runProcessCaptureOut(const std::vector<std::string>& argv, std::string& out);
