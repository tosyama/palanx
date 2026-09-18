/// palan-c2ast invocation for cinclude'd C headers.
///
/// @file		PlnGenAstC2Ast.cpp
/// @copyright	2026 YAMAGUCHI Toshinobu

#include "PlnGenAstC2Ast.h"

#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <cstring>
#include <vector>
#include "PlnGenAstMessage.h"
#include "../common/PlnProcess.h"

using std::string;
using std::vector;
using std::cerr;
using std::endl;
using std::runtime_error;
using json = nlohmann::json;
namespace fs = std::filesystem;

json execute_c2ast(const string& path_type, const string& path, const string& base_dir)
{
	fs::path exec_file_path = fs::canonical("/proc/self/exe");
	string exec_path = exec_file_path.parent_path().string();
	string c2ast_path = exec_path + "/palan-c2ast";

	vector<string> argv{c2ast_path};
	if (path_type == "inc") {
		argv.push_back("-s");
		argv.push_back(path);
	} else {
		// Local header path is relative to the including source file, not the process cwd.
		fs::path resolved = path;
		if (!resolved.is_absolute())
			resolved = fs::path(base_dir) / resolved;
		argv.push_back(resolved.string());
	}

	string result;
	PlnProcResult r = runProcessCaptureOut(argv, result);
	if (r.status == PlnSpawnStatus::SpawnFailed) {	// LCOV_EXCL_START
		cerr << "palan-c2ast: failed to execute: " << strerror(r.err_no) << endl;
		throw runtime_error(PlnGenAstMessage::getMessage(E_C2AstFailed, path));
	}	// LCOV_EXCL_STOP
	if (r.status != PlnSpawnStatus::Exited || r.exit_code != 0) {
		cerr << "palan-c2ast: exited with "
		     << (r.status == PlnSpawnStatus::Exited
		          ? std::to_string(r.exit_code)
		          : "signal " + std::to_string(r.sig_no))
		     << ": " << argv[0] << ' ' << argv.back() << endl;
		throw runtime_error(PlnGenAstMessage::getMessage(E_C2AstFailed, path));
	}

	if (result.empty()) return json{};	// LCOV_EXCL_LINE -- c2ast always prints at least {"original":...} on success
	json parsed = json::parse(result, nullptr, false);	// no exception
	if (parsed.is_discarded()) {
		cerr << "palan-c2ast: JSON parse failed" << endl;
		throw runtime_error(PlnGenAstMessage::getMessage(E_C2AstFailed, path));
	}
	return parsed;
}
