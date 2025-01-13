#include "HeaderAnalyser.h"

#include "PDBGenerator.h"

#include <iostream>
#include <string>

#include <windows.h>

int main(int argc, const char **argv) {

  HMODULE hModule = LoadLibrary(TEXT("preloader.dll"));
  if (!hModule) {
    std::cerr << "Failed to load preloader.dll\n";
    return 1;
  }

  if (argc < 5) {
    std::cerr << "Usage: " << argv[0]
              << " <build_directory> <include_header_file> <pe_file_path> "
                 "<pdb_export_path> <pdb_name>\n";
    return 1;
  }

  std::string buildDirectory = argv[1];
  std::string headerFile = argv[2];
  std::string pePath = argv[3];
  std::string pathtoexport = argv[4];
  std::string pdbName = argv[5];

  HeaderAnalyser analyser;
  auto rvaMap = analyser.analyze(buildDirectory, headerFile);

  PDBGenerator generator;
  if (!generator.generate(pathtoexport + pdbName, rvaMap, pePath)) {
    std::cerr << "Failed to generate PDB\n";
    return 1;
  }

  FreeLibrary(hModule);

  return 0;
}
