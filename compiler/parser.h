#pragma once

#include <string>
#include <vector>

#include "ast.h"

// Parses one source file into top-level statements (yoinks are left as
// Yoink nodes for the driver to expand).
std::vector<NodeP> parseFile(int file, const std::string& src);
