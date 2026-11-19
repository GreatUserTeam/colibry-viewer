// colibry-view: the Colibry OS file viewer and player on Raven.
//   colibry-view [FILE]               open a window
//   colibry-view --background FILE    start playing FILE without a window (stays after this program exits)
//   colibry-view --stop               stop that playback
#include "ViewerApp.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>

int main(int argc, char** argv) {
	bool headless = false, background = false, stop = false;
	std::filesystem::path file;
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--headless") == 0) headless = true;
		else if (std::strcmp(argv[i], "--background") == 0) background = true;
		else if (std::strcmp(argv[i], "--stop") == 0) stop = true;
		else file = argv[i];
	}

	if (stop || background) {  // no window, no Raven
		colibry::viewer::Player player;
		if (stop) {
			std::puts(player.stop() ? "Stopped." : "Nothing is playing.");
			return 0;
		}
		const auto kind = colibry::viewer::detect(file);
		if (kind != colibry::viewer::Kind::Audio && kind != colibry::viewer::Kind::Video) {
			std::fprintf(stderr, "colibry-view: %s is not an audio or video file.\n", file.string().c_str());
			return 2;
		}
		const auto r = player.start(file, kind == colibry::viewer::Kind::Video);
		if (!r.ok) { std::fprintf(stderr, "colibry-view: %s\n", r.error.c_str()); return 1; }
		std::printf("Playing with %s (pid %ld).\n", r.program.c_str(), r.pid);
		return 0;
	}

	if (!colibry::init({"Viewer", headless})) return 1;
	raven::Window window = colibry::window_from_file(colibry::sharePath("viewer/viewer.xml"));
	if (!window.is_valid()) return 1;
	colibry::viewer::ViewerApp app;
	app.attach(window);
	if (!file.empty()) app.open(file);
	return colibry::run();
}
