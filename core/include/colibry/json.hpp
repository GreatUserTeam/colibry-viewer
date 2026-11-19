// Small JSON reader/writer (MIT). Enough for config files and web API answers.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <utility>

namespace colibry {

class Json {
public:
	enum class Type { Null, Bool, Number, String, Array, Object };

	Json() = default;
	static Json boolean(bool v) { Json j; j.m_type = Type::Bool; j.m_bool = v; return j; }
	static Json number(double v) { Json j; j.m_type = Type::Number; j.m_number = v; return j; }
	static Json string(std::string v) { Json j; j.m_type = Type::String; j.m_string = std::move(v); return j; }
	static Json array() { Json j; j.m_type = Type::Array; return j; }
	static Json object() { Json j; j.m_type = Type::Object; return j; }

	// Parses `text`. On failure returns a null value and fills *error (line/column included).
	static Json parse(const std::string& text, std::string* error = nullptr);
	// Compact when indent < 0, otherwise pretty-printed with `indent` spaces.
	std::string dump(int indent = -1) const;

	Type type() const { return m_type; }
	bool isNull() const { return m_type == Type::Null; }
	bool isBool() const { return m_type == Type::Bool; }
	bool isNumber() const { return m_type == Type::Number; }
	bool isString() const { return m_type == Type::String; }
	bool isArray() const { return m_type == Type::Array; }
	bool isObject() const { return m_type == Type::Object; }

	bool asBool(bool fallback = false) const { return isBool() ? m_bool : fallback; }
	double asNumber(double fallback = 0) const { return isNumber() ? m_number : fallback; }
	int asInt(int fallback = 0) const { return isNumber() ? static_cast<int>(m_number) : fallback; }
	std::string asString(const std::string& fallback = {}) const { return isString() ? m_string : fallback; }

	std::size_t size() const { return isArray() ? m_items.size() : isObject() ? m_members.size() : 0; }
	// Missing keys and indexes give a null value, so chains like j["a"]["b"][0] never fail.
	const Json& operator[](const std::string& key) const;
	const Json& operator[](std::size_t index) const;
	bool has(const std::string& key) const { return isObject() && m_members.count(key) > 0; }
	const std::map<std::string, Json>& members() const { return m_members; }
	const std::vector<Json>& items() const { return m_items; }

	Json& set(const std::string& key, Json value) { m_type = Type::Object; m_members[key] = std::move(value); return *this; }
	Json& push(Json value) { m_type = Type::Array; m_items.push_back(std::move(value)); return *this; }

private:
	Type m_type = Type::Null;
	bool m_bool = false;
	double m_number = 0;
	std::string m_string;
	std::vector<Json> m_items;
	std::map<std::string, Json> m_members;
};

}  // namespace colibry
