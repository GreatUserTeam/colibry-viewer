// Audio and video playback through an external program (MIT). Raven has no decoder, so the player is
// mpv, ffplay or paplay, started without a shell and detached from this program. That is what the
// background mode is: `colibry-view --background FILE` starts playback and exits; `colibry-view --stop` ends it.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace colibry::viewer {

struct PlayerResult {
	bool ok = false;
	std::string error;
	std::string program;  // what was started
	long pid = 0;
};

class Player {
public:
	// `programs`: candidates in order of preference (the first one found in PATH is used).
	// `runtimeDir`: where the pid file lives; empty = $XDG_RUNTIME_DIR, else /tmp.
	explicit Player(std::vector<std::string> programs = defaultPrograms(), std::filesystem::path runtimeDir = {});

	static std::vector<std::string> defaultPrograms() { return {"mpv", "ffplay", "paplay"}; }

	// The command line for a program; the file is always its own argument, after "--" where the program allows it.
	static std::vector<std::string> buildArgv(const std::string& program, const std::filesystem::path& file, bool video);

	// Starts playing (an earlier playback is stopped first). Needs one of the programs to be installed.
	PlayerResult start(const std::filesystem::path& file, bool video = false);
	// Ends the playback started by this or an earlier run. False when nothing was playing.
	bool stop();
	// pid of the running player, 0 when none. A stale pid file (process gone, or reused by another
	// program) is removed.
	long running();

	const std::filesystem::path& pidFile() const { return m_pidFile; }

private:
	std::vector<std::string> m_programs;
	std::filesystem::path m_pidFile;
};

}  // namespace colibry::viewer
