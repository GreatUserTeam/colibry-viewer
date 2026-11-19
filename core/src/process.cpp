#include <colibry/process.hpp>

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

extern char** environ;

namespace colibry {

ProcessResult runProcess(const std::vector<std::string>& argv) {
	ProcessResult result;
	if (argv.empty()) return result;

	int fds[2];
	if (pipe(fds) != 0) return result;

	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_adddup2(&actions, fds[1], 1);
	posix_spawn_file_actions_adddup2(&actions, fds[1], 2);
	posix_spawn_file_actions_addclose(&actions, fds[0]);
	posix_spawn_file_actions_addclose(&actions, fds[1]);

	std::vector<char*> args;
	for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
	args.push_back(nullptr);

	pid_t pid = 0;
	const int rc = posix_spawnp(&pid, args[0], &actions, nullptr, args.data(), environ);
	posix_spawn_file_actions_destroy(&actions);
	close(fds[1]);
	if (rc != 0) {
		close(fds[0]);
		return result;
	}
	result.started = true;

	char buf[4096];
	ssize_t n;
	while ((n = read(fds[0], buf, sizeof(buf))) > 0) result.output.append(buf, static_cast<std::size_t>(n));
	close(fds[0]);

	int status = 0;
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) result.exitCode = WEXITSTATUS(status);
	return result;
}

bool programExists(const std::string& program) {
	if (program.find('/') != std::string::npos) return access(program.c_str(), X_OK) == 0;
	const char* path = std::getenv("PATH");
	if (!path) return false;
	std::stringstream parts(path);
	std::string dir;
	while (std::getline(parts, dir, ':')) {
		if (dir.empty()) continue;
		if (access((std::filesystem::path(dir) / program).c_str(), X_OK) == 0) return true;
	}
	return false;
}

}  // namespace colibry
