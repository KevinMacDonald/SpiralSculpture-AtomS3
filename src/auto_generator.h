#pragma once

#include <vector>
#include <string>

namespace AutoGenerator {

// Generates a script of commands for a given duration in minutes.
std::vector<std::string> generateScript(int duration_minutes);

// Generates a script for the "steady rotate" mode for a given duration.
std::vector<std::string> generateSteadyRotateScript(int duration_minutes);

} // namespace AutoGenerator