#include "TextLoad.hpp"

#include "FileKind.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <system_error>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace colibry::viewer {
namespace {

bool validUtf8(const std::string& s, bool allowCutEnd) {
	std::size_t i = 0;
	while (i < s.size()) {
		const auto c = static_cast<unsigned char>(s[i]);
		const std::size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
		if (n == 0) return false;
		if (i + n > s.size()) return allowCutEnd;
		for (std::size_t k = 1; k < n; ++k) if ((static_cast<unsigned char>(s[i + k]) >> 6) != 0x2) return false;
		i += n;
	}
	return true;
}

std::string latin1ToUtf8(const std::string& s) {
	std::string out;
	for (unsigned char c : s) {
		if (c < 0x80) out += static_cast<char>(c);
		else { out += static_cast<char>(0xC0 | (c >> 6)); out += static_cast<char>(0x80 | (c & 0x3F)); }
	}
	return out;
}

bool readPrefix(const fs::path& path, std::uintmax_t limit, std::string* data, std::uintmax_t* size, std::string* error) {
	const Kind kind = detect(path);
	if (kind == Kind::Missing) { *error = "The file does not exist."; return false; }
	if (kind == Kind::Directory) { *error = "This is a folder."; return false; }
	if (kind == Kind::Special) { *error = "This is a device or a pipe; it cannot be shown."; return false; }
	std::error_code ec;
	*size = fs::file_size(path, ec);
	std::ifstream in(path, std::ios::binary);
	if (!in) { *error = "The file cannot be opened."; return false; }
	data->assign(static_cast<std::size_t>(std::min<std::uintmax_t>(limit, *size)), '\0');
	in.read(data->data(), static_cast<std::streamsize>(data->size()));
	data->resize(static_cast<std::size_t>(in.gcount()));
	return true;
}

}  // namespace

Loaded loadText(const fs::path& path, std::uintmax_t limit) {
	Loaded r;
	std::string data;
	if (!readPrefix(path, limit, &data, &r.fileSize, &r.error)) return r;
	r.truncated = r.fileSize > data.size();
	if (data.compare(0, 3, "\xEF\xBB\xBF") == 0) data.erase(0, 3);
	if (validUtf8(data, r.truncated)) {
		if (r.truncated) {  // drop a character that was cut in two
			std::size_t cut = data.size();
			while (cut > 0 && (static_cast<unsigned char>(data[cut - 1]) & 0xC0) == 0x80) --cut;
			if (cut > 0 && !validUtf8(data.substr(cut - 1), false)) data.resize(cut - 1);
		}
	} else {
		data = latin1ToUtf8(data);
	}
	for (std::size_t i = 0; i < data.size(); ++i) {
		if (data[i] == '\r' && i + 1 < data.size() && data[i + 1] == '\n') continue;
		r.text += data[i];
	}
	r.ok = true;
	return r;
}

Loaded loadHex(const fs::path& path, std::uintmax_t limit) {
	Loaded r;
	std::string data;
	if (!readPrefix(path, limit, &data, &r.fileSize, &r.error)) return r;
	r.truncated = r.fileSize > data.size();
	r.text = hexDump(data);
	r.ok = true;
	return r;
}

std::string hexDump(const std::string& data, std::uint64_t base) {
	std::string out;
	char buf[16];
	for (std::size_t line = 0; line < data.size(); line += 16) {
		std::snprintf(buf, sizeof(buf), "%08llx ", static_cast<unsigned long long>(base + line));
		out += buf;
		std::string ascii;
		for (std::size_t i = 0; i < 16; ++i) {
			if (i == 8) out += ' ';
			if (line + i < data.size()) {
				const auto c = static_cast<unsigned char>(data[line + i]);
				std::snprintf(buf, sizeof(buf), " %02x", c);
				out += buf;
				ascii += (c >= 0x20 && c < 0x7F) ? static_cast<char>(c) : '.';
			} else {
				out += "   ";
			}
		}
		out += "  |" + ascii + "|\n";
	}
	return out;
}

std::vector<fs::path> siblings(const fs::path& path) {
	std::vector<fs::path> files;
	std::error_code ec;
	const fs::path dir = path.has_parent_path() ? path.parent_path() : fs::path(".");
	for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
		const std::string name = it->path().filename().string();
		if (name.empty() || name[0] == '.') continue;
		if (it->is_regular_file(ec)) files.push_back(it->path());
	}
	auto key = [](const fs::path& p) {
		std::string s = p.filename().string();
		for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	};
	std::sort(files.begin(), files.end(), [&](const fs::path& a, const fs::path& b) {
		const std::string ka = key(a), kb = key(b);
		return ka != kb ? ka < kb : a < b;
	});
	return files;
}

}  // namespace colibry::viewer
