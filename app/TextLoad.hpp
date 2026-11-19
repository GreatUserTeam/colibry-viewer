// Reading a file for display: bounded, with encoding repair; hex dump; sibling files (MIT).
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace colibry::viewer {

constexpr std::uintmax_t kMaxTextBytes = 4 * 1024 * 1024;  // bigger files are cut here
constexpr std::uintmax_t kMaxHexBytes = 64 * 1024;

struct Loaded {
	bool ok = false;
	std::string error;
	std::string text;
	bool truncated = false;       // the file is longer than what is shown
	std::uintmax_t fileSize = 0;
};

// UTF-8 text. A BOM is dropped, CRLF becomes LF, invalid UTF-8 is read as an 8-bit encoding,
// and a cut never lands inside a multi-byte character.
Loaded loadText(const std::filesystem::path& path, std::uintmax_t limit = kMaxTextBytes);
// First `limit` bytes as a hex dump.
Loaded loadHex(const std::filesystem::path& path, std::uintmax_t limit = kMaxHexBytes);

// "00000010  48 65 6c 6c 6f 20 77 6f  72 6c 64 0a              |Hello world.|" , 16 bytes per line.
std::string hexDump(const std::string& data, std::uint64_t baseOffset = 0);

// Regular files in the folder of `path`, hidden ones left out, sorted without regard to case.
std::vector<std::filesystem::path> siblings(const std::filesystem::path& path);

}  // namespace colibry::viewer
