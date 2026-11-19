#include <colibry/json.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

namespace colibry {
namespace {

const Json kNull;

struct Parser {
	const std::string& s;
	std::size_t i = 0;
	std::string error;

	explicit Parser(const std::string& text) : s(text) {}

	void skip() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i; }
	bool fail(const std::string& what) {
		if (error.empty()) {
			int line = 1, col = 1;
			for (std::size_t k = 0; k < i && k < s.size(); ++k) { if (s[k] == '\n') { ++line; col = 1; } else ++col; }
			error = what + " at line " + std::to_string(line) + ", column " + std::to_string(col);
		}
		return false;
	}

	static void putUtf8(std::string& out, unsigned cp) {
		if (cp < 0x80) out += static_cast<char>(cp);
		else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
		else if (cp < 0x10000) { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
		else { out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
	}

	bool hex4(unsigned& v) {
		if (i + 4 > s.size()) return fail("bad \\u escape");
		v = 0;
		for (int k = 0; k < 4; ++k) {
			char c = s[i++];
			v <<= 4;
			if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
			else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
			else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
			else return fail("bad \\u escape");
		}
		return true;
	}

	bool str(std::string& out) {
		++i;  // opening quote
		while (i < s.size()) {
			char c = s[i++];
			if (c == '"') return true;
			if (static_cast<unsigned char>(c) < 0x20) return fail("control character in string");
			if (c != '\\') { out += c; continue; }
			if (i >= s.size()) break;
			switch (char e = s[i++]) {
				case '"': out += '"'; break;
				case '\\': out += '\\'; break;
				case '/': out += '/'; break;
				case 'b': out += '\b'; break;
				case 'f': out += '\f'; break;
				case 'n': out += '\n'; break;
				case 'r': out += '\r'; break;
				case 't': out += '\t'; break;
				case 'u': {
					unsigned cp;
					if (!hex4(cp)) return false;
					if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u') {
						i += 2;
						unsigned lo;
						if (!hex4(lo)) return false;
						cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
					}
					putUtf8(out, cp);
					break;
				}
				default: (void)e; return fail("bad escape");
			}
		}
		return fail("unterminated string");
	}

	bool value(Json& out, int depth) {
		if (depth > 200) return fail("nested too deeply");
		skip();
		if (i >= s.size()) return fail("unexpected end");
		char c = s[i];
		if (c == '{') {
			++i;
			out = Json::object();
			skip();
			if (i < s.size() && s[i] == '}') { ++i; return true; }
			while (true) {
				skip();
				if (i >= s.size() || s[i] != '"') return fail("expected a key");
				std::string key;
				if (!str(key)) return false;
				skip();
				if (i >= s.size() || s[i] != ':') return fail("expected ':'");
				++i;
				Json v;
				if (!value(v, depth + 1)) return false;
				out.set(key, std::move(v));
				skip();
				if (i < s.size() && s[i] == ',') { ++i; continue; }
				if (i < s.size() && s[i] == '}') { ++i; return true; }
				return fail("expected ',' or '}'");
			}
		}
		if (c == '[') {
			++i;
			out = Json::array();
			skip();
			if (i < s.size() && s[i] == ']') { ++i; return true; }
			while (true) {
				Json v;
				if (!value(v, depth + 1)) return false;
				out.push(std::move(v));
				skip();
				if (i < s.size() && s[i] == ',') { ++i; continue; }
				if (i < s.size() && s[i] == ']') { ++i; return true; }
				return fail("expected ',' or ']'");
			}
		}
		if (c == '"') { std::string v; if (!str(v)) return false; out = Json::string(std::move(v)); return true; }
		if (s.compare(i, 4, "true") == 0) { i += 4; out = Json::boolean(true); return true; }
		if (s.compare(i, 5, "false") == 0) { i += 5; out = Json::boolean(false); return true; }
		if (s.compare(i, 4, "null") == 0) { i += 4; out = Json(); return true; }
		if (c == '-' || (c >= '0' && c <= '9')) {
			const char* start = s.c_str() + i;
			char* end = nullptr;
			double v = std::strtod(start, &end);
			if (end == start) return fail("bad number");
			i += static_cast<std::size_t>(end - start);
			out = Json::number(v);
			return true;
		}
		return fail("unexpected character");
	}
};

void quote(std::string& out, const std::string& s) {
	out += '"';
	for (unsigned char c : s) {
		switch (c) {
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (c < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", c); out += b; }
				else out += static_cast<char>(c);
		}
	}
	out += '"';
}

void write(const Json& j, std::string& out, int indent, int level) {
	const auto nl = [&](int lv) { if (indent >= 0) { out += '\n'; out.append(static_cast<std::size_t>(lv * indent), ' '); } };
	switch (j.type()) {
		case Json::Type::Null: out += "null"; break;
		case Json::Type::Bool: out += j.asBool() ? "true" : "false"; break;
		case Json::Type::Number: {
			double v = j.asNumber();
			char b[40];
			if (std::floor(v) == v && std::fabs(v) < 1e15) std::snprintf(b, sizeof(b), "%.0f", v);
			else std::snprintf(b, sizeof(b), "%.17g", v);
			out += b;
			break;
		}
		case Json::Type::String: quote(out, j.asString()); break;
		case Json::Type::Array:
			if (j.size() == 0) { out += "[]"; break; }
			out += '[';
			for (std::size_t k = 0; k < j.size(); ++k) { if (k) out += ','; nl(level + 1); write(j[k], out, indent, level + 1); }
			nl(level);
			out += ']';
			break;
		case Json::Type::Object: {
			if (j.size() == 0) { out += "{}"; break; }
			out += '{';
			bool first = true;
			for (const auto& [key, value] : j.members()) {
				if (!first) out += ',';
				first = false;
				nl(level + 1);
				quote(out, key);
				out += indent >= 0 ? ": " : ":";
				write(value, out, indent, level + 1);
			}
			nl(level);
			out += '}';
			break;
		}
	}
}

}  // namespace

Json Json::parse(const std::string& text, std::string* error) {
	Parser p(text);
	Json out;
	bool ok = p.value(out, 0);
	if (ok) { p.skip(); if (p.i != text.size()) ok = p.fail("extra text after the value"); }
	if (!ok) { if (error) *error = p.error; return Json(); }
	if (error) error->clear();
	return out;
}

std::string Json::dump(int indent) const {
	std::string out;
	write(*this, out, indent, 0);
	return out;
}

const Json& Json::operator[](const std::string& key) const {
	if (!isObject()) return kNull;
	auto it = m_members.find(key);
	return it == m_members.end() ? kNull : it->second;
}

const Json& Json::operator[](std::size_t index) const {
	return isArray() && index < m_items.size() ? m_items[index] : kNull;
}

}  // namespace colibry
