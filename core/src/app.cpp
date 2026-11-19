#include <colibry/app.hpp>

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <utility>

namespace colibry {

bool init(const AppOptions& options) {
	RInitDesc desc{};
	desc.backend = options.headless ? R_BACKEND_NULL : R_BACKEND_VULKAN;
	desc.headless = options.headless ? R_TRUE : R_FALSE;
	desc.app_name = options.name;
	if (!raven::init_ex(&desc)) return false;
	// XML mistakes and other library problems go to stderr instead of vanishing.
	Rerror_set_callback([](RError, RSeverity, const char* detail) { std::fprintf(stderr, "raven: %s\n", detail ? detail : "?"); });
	raven::action_bind("app.quit", [](Rid, const char*) { raven::quit(0); });
	return true;
}

std::string sharePath(const std::string& relative) {
	namespace fs = std::filesystem;
	std::error_code ec;
	std::vector<fs::path> roots;
	if (const char* env = std::getenv("COLIBRY_SHARE")) {
		return std::string(env) + "/" + relative;  // an explicit choice is never second-guessed
	}
	const fs::path exe = fs::read_symlink("/proc/self/exe", ec);  // <prefix>/bin/app -> <prefix>/share/colibry
	if (!ec) roots.push_back(exe.parent_path().parent_path() / "share" / "colibry");
	roots.push_back("/usr/local/share/colibry");
	roots.push_back("/usr/share/colibry");
	for (const fs::path& r : roots)
		if (fs::exists(r / relative, ec)) return (r / relative).string();
	return "/usr/share/colibry/" + relative;
}

raven::Window window_from_file(const std::string& path) {
	return raven::Window::create_from_xml_file(path.c_str());
}

raven::Window window_from_string(const std::string& xml) {
	return raven::Window::create_from_xml(xml.data(), xml.size());
}

void post(std::function<void()> task) {
	auto* heap = new std::function<void()>(std::move(task));
	Rtask_post(
	    [](void* user) {
		    std::unique_ptr<std::function<void()>> owned(static_cast<std::function<void()>*>(user));
		    (*owned)();
	    },
	    heap);
}

int run() {
	while (raven::wait_frame(-1.0)) {}
	raven::shutdown();
	return 0;
}

}  // namespace colibry
