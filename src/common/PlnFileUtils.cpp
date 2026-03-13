#include <iostream>
#include <filesystem>
#include <random>
#include <chrono>

namespace fs = std::filesystem;
using namespace std::chrono;

/// Delete files older than the specified days.
void cleanOldTempFiles(const fs::path& tempDir, const std::string& prefix, int days) {
	auto now = system_clock::now();
	for (const auto& entry : fs::directory_iterator(tempDir)) {
		if (!entry.is_regular_file()) continue; // ignore directories

		fs::path filePath = entry.path();
		if (filePath.filename().string().find(prefix) != 0) continue;

		auto ftime = fs::last_write_time(filePath);
		auto fileTime = system_clock::time_point(ftime.time_since_epoch());
		auto age = duration_cast<hours>(now - fileTime).count() / 24;

		if (age >= days) {
		 	try {
		 		fs::remove(filePath);
		 		// TODO: Write log.
		 	} catch (const std::exception& e) {
		 		// TODO: Write error log.
		 	}
		}
	}
}

/// Get a unique temporary file name.
std::string getUniqueTempFileName(const std::string& prefix, const std::string& dirname) {
	fs::path tempDir = fs::temp_directory_path() / dirname;
	if (!fs::exists(tempDir)) {
		fs::create_directories(tempDir);
	}
	cleanOldTempFiles(tempDir, prefix, 5);

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<int> dist(100000, 999999);

	fs::path tempFilePath;
	do {
		tempFilePath = tempDir / (prefix + std::to_string(dist(gen)) + ".tmp");
	} while (fs::exists(tempFilePath));

	return tempFilePath.string();
}

