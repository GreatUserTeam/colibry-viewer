// Colibry OS application base on Raven (MIT).
// Shared start-up, XML window loading and the screen-edge inset used by every app.
#pragma once

#include <raven.hpp>

#include <functional>
#include <string>

namespace colibry {

constexpr const char* kBrand = "Colibry OS";
constexpr float kScreenInset = 16.0f;  // logical px between content and window edge

struct AppOptions {
	const char* name = "app";
	bool headless = false;  // null backend: no window system, no GPU (tests, CI)
};

// Starts Raven once. Returns false when no backend could be started.
bool init(const AppOptions& options);

// Path of a shared file: $COLIBRY_SHARE/<relative> or /usr/share/colibry/<relative>.
std::string sharePath(const std::string& relative);

// Creates a window from an XML file, or from a string.
raven::Window window_from_file(const std::string& path);
raven::Window window_from_string(const std::string& xml);

// Runs `task` once on the UI thread. Safe from any thread and for any number of pending tasks.
// (raven::task_post(std::function) is not: the binding keeps a single callable for all calls, unlocked.)
void post(std::function<void()> task);

// Runs until the window is closed or app.quit fires.
int run();

}  // namespace colibry
