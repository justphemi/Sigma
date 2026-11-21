#pragma once

#include <string>

#include "sema.h"

// Turns an analysed program into one self-contained C file (runtime included).
std::string generateC(Program& prog, const std::string& runtime);
