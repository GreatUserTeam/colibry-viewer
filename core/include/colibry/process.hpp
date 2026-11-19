// Runs a program without a shell (MIT). Arguments are passed as they are: no quoting problems.
#pragma once

#include <string>
#include <vector>

namespace colibry {

struct ProcessResult {
	bool started = false;  // false: the program was not found or could not be started
	int exitCode = -1;     // exit status; -1 when it was killed by a signal or not started
	std::string output;    // stdout and stderr together
};

// argv[0] is looked up in PATH. Blocks until the program ends.
ProcessResult runProcess(const std::vector<std::string>& argv);

// True when `program` can be found in PATH.
bool programExists(const std::string& program);

}  // namespace colibry
