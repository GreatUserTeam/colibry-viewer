#include "FileKind.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <system_error>
#include <cctype>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace colibry::viewer {

const char* kindName(Kind k) {
	switch (k) {
		case Kind::Text: return "Text";
		case Kind::Image: return "Image";
		case Kind::Audio: return "Audio";
		case Kind::Video: return "Video";
		case Kind::Binary: return "Binary";
		case Kind::Directory: return "Folder";
		case Kind::Special: return "Special file";
		case Kind::Missing: return "Missing";
	}
	return "";
}

static std::string lower(std::string s) {
	for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return s;
}

Kind kindOfExtension(const std::string& extension) {
	const std::string e = lower(extension[0] == '.' ? extension.substr(1) : extension);
	for (const char* x : {"png", "jpg", "jpeg", "gif", "bmp", "webp", "svg", "tga"}) if (e == x) return Kind::Image;
	for (const char* x : {"mp3", "ogg", "oga", "wav", "flac", "m4a", "opus", "aac"}) if (e == x) return Kind::Audio;
	for (const char* x : {"mp4", "mkv", "webm", "avi", "mov", "m4v"}) if (e == x) return Kind::Video;
	return Kind::Binary;
}

bool hasImageMagic(const std::string& h) {
	auto starts = [&](const char* m, std::size_t n) { return h.size() >= n && h.compare(0, n, m, n) == 0; };
	if (starts("\x89PNG\r\n\x1a\n", 8) || starts("\xFF\xD8\xFF", 3) || starts("GIF87a", 6) || starts("GIF89a", 6) || starts("BM", 2)) return true;
	return h.size() >= 12 && h.compare(0, 4, "RIFF") == 0 && h.compare(8, 4, "WEBP") == 0;
}

bool looksLikeText(const std::string& s) {
	if (s.empty()) return true;
	std::size_t control = 0;
	for (unsigned char c : s) {
		if (c == 0) return false;
		if (c < 0x20 && c != '\n' && c != '\r' && c != '\t' && c != '\f' && c != 0x1B) ++control;
	}
	return control * 100 <= s.size() * 2;  // at most 2 % control characters
}

Kind detect(const fs::path& path) {
	std::error_code ec;
	const fs::file_status st = fs::status(path, ec);
	if (ec || !fs::exists(st)) return Kind::Missing;
	if (fs::is_directory(st)) return Kind::Directory;
	if (!fs::is_regular_file(st)) return Kind::Special;

	const Kind byName = kindOfExtension(path.extension().string());
	if (byName == Kind::Audio || byName == Kind::Video) return byName;

	std::ifstream in(path, std::ios::binary);
	std::string head(kSampleSize, '\0');
	in.read(head.data(), static_cast<std::streamsize>(head.size()));
	head.resize(static_cast<std::size_t>(in.gcount()));
	if (hasImageMagic(head)) return Kind::Image;
	if (lower(path.extension().string()) == ".svg") return Kind::Image;
	return looksLikeText(head) ? Kind::Text : Kind::Binary;
}

std::string humanSize(std::uintmax_t bytes) {
	if (bytes < 1024) return std::to_string(bytes) + (bytes == 1 ? " byte" : " bytes");
	static const char* units[] = {"KB", "MB", "GB", "TB"};
	double v = static_cast<double>(bytes);
	int i = -1;
	while (v >= 1024.0 && i < 3) { v /= 1024.0; ++i; }
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%.1f %s", v, units[i]);
	return buf;
}

}  // namespace colibry::viewer
