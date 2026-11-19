// Headless tests: no window system and no GPU are needed (Raven's null backend).
#include <colibry/app.hpp>
#include <colibry/json.hpp>
#include <colibry/process.hpp>

#include "check.hpp"

#include <cstring>
#include <cstdio>
#include <string>

static const char* kXml =
    "<Window Title=\"T\" Width=\"300\" Height=\"200\">"
    "<VBox Slot.Padding=\"16\"><Label Name=\"time\" Text=\"00:00\"/>"
    "<Button Name=\"quit\" Text=\"Close\" OnClicked=\"app.quit\"/></VBox></Window>";

static int testJson() {
	using colibry::Json;
	std::string err;
	Json j = Json::parse(R"({"a":[1,2.5,{"b":"xé\n"}],"t":true,"n":null,"s":"😀"})", &err);
	EXPECT(err.empty());
	EXPECT(j["a"][1].asNumber() == 2.5 && j["a"][2]["b"].asString() == "x\xC3\xA9\n");
	EXPECT(j["t"].asBool() && j["n"].isNull() && j["s"].asString() == "\xF0\x9F\x98\x80");
	EXPECT(j["missing"]["deep"][3].isNull());  // chains never fail
	EXPECT(Json::parse(j.dump()).dump() == j.dump());
	EXPECT(Json::parse(j.dump(2)).dump() == j.dump());
	Json::parse("{\"a\":", &err);
	EXPECT(!err.empty());
	Json::parse("[1,]", &err);
	EXPECT(!err.empty());
	Json::parse("{} x", &err);
	EXPECT(!err.empty());
	EXPECT(Json::parse(std::string(500, '[') + std::string(500, ']'), &err).isNull() && !err.empty());
	Json o = Json::object();
	o.set("k", Json::string("v\"q")).set("n", Json::number(3));
	EXPECT(o.dump() == "{\"k\":\"v\\\"q\",\"n\":3}");
	return 0;
}

static int testProcess() {
	auto r = colibry::runProcess({"printf", "%s-%s", "a b", "c"});
	EXPECT(r.started && r.exitCode == 0 && r.output == "a b-c");
	EXPECT(colibry::runProcess({"false"}).exitCode == 1);
	EXPECT(!colibry::runProcess({"definitely-not-a-program-xyz"}).started);
	EXPECT(colibry::programExists("sh") && !colibry::programExists("definitely-not-a-program-xyz"));
	return 0;
}

int main() {
	EXPECT(std::strcmp(colibry::kBrand, "Colibry OS") == 0);
	EXPECT(colibry::kScreenInset > 0.0f);
	RUN(testJson());
	RUN(testProcess());

	EXPECT(colibry::init({"test", true}));
	raven::Window window = colibry::window_from_string(kXml);
	EXPECT(window.is_valid());
	raven::Widget root(window.get_content());
	EXPECT(raven::Widget(root.find("time")).is_valid());
	EXPECT(raven::Widget(root.find("quit")).is_valid());
	EXPECT(!raven::Widget(root.find("missing")).is_valid());
	raven::Label(root.find("time")).set_text("12:34");
	raven::draw_frame();
	raven::shutdown();
	std::printf("base: all checks passed\n");
	return 0;
}
