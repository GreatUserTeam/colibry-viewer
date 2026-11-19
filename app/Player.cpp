#include "Player.hpp"

#include <colibry/process.hpp>

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

extern char** environ;

namespace fs = std::filesystem;

namespace colibry::viewer {
namespace {

std::string baseName(const std::string& s) {
	const auto slash = s.rfind('/');
	return slash == std::string::npos ? s : s.substr(slash + 1);
}

// First word of /proc/<pid>/cmdline, empty when the process is gone. Zombies count as gone.
std::string commandOf(long pid) {
	std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
	std::string line;
	std::getline(stat, line);
	const auto close = line.rfind(')');
	if (close == std::string::npos || close + 2 >= line.size() || line[close + 2] == 'Z') return {};
	std::ifstream in("/proc/" + std::to_string(pid) + "/cmdline", std::ios::binary);
	std::string cmd;
	std::getline(in, cmd, '\0');
	return cmd;
}

}  // namespace

Player::Player(std::vector<std::string> programs, fs::path runtimeDir) : m_programs(std::move(programs)) {
	if (runtimeDir.empty()) {
		const char* xdg = std::getenv("XDG_RUNTIME_DIR");
		runtimeDir = (xdg && *xdg) ? fs::path(xdg) : fs::path("/tmp");
	}
	m_pidFile = runtimeDir / ("colibry-view-" + std::to_string(::getuid()) + ".pid");
}

std::vector<std::string> Player::buildArgv(const std::string& program, const fs::path& file, bool video) {
	const std::string name = baseName(program);
	if (name == "mpv") {
		std::vector<std::string> a = {program, "--really-quiet", "--no-terminal"};
		if (!video) a.push_back("--no-video");
		a.push_back("--");
		a.push_back(file.string());
		return a;
	}
	if (name == "ffplay") {
		std::vector<std::string> a = {program, "-autoexit", "-loglevel", "quiet"};
		if (!video) a.push_back("-nodisp");
		a.push_back("-i");
		a.push_back(file.string());
		return a;
	}
	if (name == "paplay") return {program, "--", file.string()};
	return {program, file.string()};
}

long Player::running() {
	std::ifstream in(m_pidFile);
	long pid = 0;
	if (!(in >> pid) || pid <= 1) {
		std::error_code ec;
		fs::remove(m_pidFile, ec);
		return 0;
	}
	const std::string cmd = commandOf(pid);
	for (const auto& p : m_programs)
		if (!cmd.empty() && baseName(cmd) == baseName(p)) return pid;
	std::error_code ec;  // gone, or the number now belongs to some other program: never touch that
	fs::remove(m_pidFile, ec);
	return 0;
}

bool Player::stop() {
	const long pid = running();
	if (pid == 0) return false;
	::kill(static_cast<pid_t>(pid), SIGTERM);
	for (int i = 0; i < 100; ++i) {  // up to a second
		int st = 0;
		::waitpid(static_cast<pid_t>(pid), &st, WNOHANG);  // reaps it when it is our own child
		if (commandOf(pid).empty()) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	if (!commandOf(pid).empty()) {
		::kill(static_cast<pid_t>(pid), SIGKILL);
		int st = 0;
		::waitpid(static_cast<pid_t>(pid), &st, 0);
	}
	std::error_code ec;
	fs::remove(m_pidFile, ec);
	return true;
}

PlayerResult Player::start(const fs::path& file, bool video) {
	PlayerResult r;
	std::string program;
	for (const auto& p : m_programs) if (programExists(p)) { program = p; break; }
	if (program.empty()) {
		r.error = "No player found. Install mpv, ffplay (ffmpeg) or paplay.";
		return r;
	}
	stop();

	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
	posix_spawn_file_actions_addopen(&actions, 1, "/dev/null", O_WRONLY, 0);
	posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);
	posix_spawnattr_t attr;
	posix_spawnattr_init(&attr);
	posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID);  // its own session: closing the window does not stop it

	const std::vector<std::string> argv = buildArgv(program, file, video);
	std::vector<char*> args;
	for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
	args.push_back(nullptr);

	pid_t pid = 0;
	const int rc = posix_spawnp(&pid, args[0], &actions, &attr, args.data(), environ);
	posix_spawn_file_actions_destroy(&actions);
	posix_spawnattr_destroy(&attr);
	if (rc != 0) { r.error = "Cannot start " + program + "."; return r; }

	std::error_code ec;
	fs::create_directories(m_pidFile.parent_path(), ec);
	std::ofstream(m_pidFile) << pid << "\n";
	r.ok = true;
	r.program = program;
	r.pid = pid;
	return r;
}

}  // namespace colibry::viewer
