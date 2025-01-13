#pragma once

#include <string>
#include <unordered_map>

class HeaderAnalyser {
public:
  /**
   * @param buildPath the path of compile_commands.json
   * @param sourceFile the include file to analyze
   * @return a map of mangled names to RVAs
   */
  std::unordered_map<std::string, int64_t>
  analyze(const std::string &buildPath, const std::string &sourceFile);
};
