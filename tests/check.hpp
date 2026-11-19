// Tiny test helpers shared by tests/test_*.cpp (MIT).
#pragma once

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

#define EXPECT(c) do { if (!(c)) { std::printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)
#define RUN(call) do { if (int rc_ = (call)) return rc_; } while (0)

#ifndef COLIBRY_SOURCE_DIR
#define COLIBRY_SOURCE_DIR "."
#endif

// A fresh directory that is removed when the object goes away.
struct TempDir {
	std::filesystem::path path;
	explicit TempDir(const std::string& tag) {
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		path = std::filesystem::temp_directory_path() / ("colibry-" + tag + "-" + std::to_string(stamp));
		std::filesystem::create_directories(path);
	}
	~TempDir() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};
