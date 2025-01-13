#pragma once

#include <llvm/DebugInfo/PDB/Native/InfoStreamBuilder.h>

#include <string>
#include <unordered_map>

class PDBGenerator {
public:
  bool generate(const std::string &OutputPdbPath,
                const std::unordered_map<std::string, int64_t> &Symbols,
                const std::string &PePath);

  std::pair<llvm::codeview::GUID, uint32_t>
  extractGUIDAndAge(const std::string &PePath);
};
