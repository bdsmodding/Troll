#include "PDBGenerator.h"

#include "llvm/DebugInfo/MSF/MSFBuilder.h"
#include "llvm/DebugInfo/PDB/Native/DbiStreamBuilder.h"
#include "llvm/DebugInfo/PDB/Native/GSIStreamBuilder.h"
#include "llvm/DebugInfo/PDB/Native/InfoStreamBuilder.h"
#include "llvm/DebugInfo/PDB/Native/PDBFileBuilder.h"
#include "llvm/DebugInfo/PDB/Native/TpiStreamBuilder.h"
#include "llvm/Object/COFF.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"

#include <algorithm>
#include <cstring>

using namespace llvm;
using namespace llvm::object;

struct CVInfoPDB {
  uint32_t CvSignature;
  uint8_t GUID[16];
  uint32_t Age;
  std::string PdbFileName;
};

std::pair<llvm::codeview::GUID, uint32_t>
PDBGenerator::extractGUIDAndAge(const std::string &PePath) {

  auto BufferOrError = MemoryBuffer::getFile(PePath);
  if (!BufferOrError) {
    llvm::errs() << "Failed to open file: " << PePath << "\n";
    return {};
  }

  Expected<std::unique_ptr<ObjectFile>> ObjOrErr =
      ObjectFile::createObjectFile(**BufferOrError);
  if (!ObjOrErr) {
    llvm::errs() << "Failed to create object file: "
                 << llvm::toString(ObjOrErr.takeError()) << "\n";
    return {};
  }

  if (auto *COFF = dyn_cast<COFFObjectFile>(ObjOrErr->get())) {
    for (const auto &DebugDir : COFF->debug_directories()) {
      if (DebugDir.Type != COFF::IMAGE_DEBUG_TYPE_CODEVIEW)
        continue;

      uint64_t RawDataAddr = DebugDir.PointerToRawData;
      uint64_t SizeOfData = DebugDir.SizeOfData;

      StringRef DebugData = COFF->getMemoryBufferRef().getBuffer().slice(
          RawDataAddr, RawDataAddr + SizeOfData);
      if (DebugData.size() < sizeof(CVInfoPDB)) {
        llvm::errs() << "Debug data too small!\n";
        return {};
      }

      const auto *CVInfo =
          reinterpret_cast<const CVInfoPDB *>(DebugData.data());
      if (CVInfo->CvSignature == 0x53445352) {
        return std::make_pair(
            codeview::GUID{{CVInfo->GUID[0], CVInfo->GUID[1], CVInfo->GUID[2],
                            CVInfo->GUID[3], CVInfo->GUID[4], CVInfo->GUID[5],
                            CVInfo->GUID[6], CVInfo->GUID[7], CVInfo->GUID[8],
                            CVInfo->GUID[9], CVInfo->GUID[10], CVInfo->GUID[11],
                            CVInfo->GUID[12], CVInfo->GUID[13],
                            CVInfo->GUID[14], CVInfo->GUID[15]}},
            CVInfo->Age);
      } else {
        llvm::errs() << "Unknown CvSignature!\n";
        return {};
      }
    }
  } else {
    llvm::errs() << "Not a valid PE file!\n";
    return {};
  }

  return {};
}

bool PDBGenerator::generate(
    const std::string &OutputPdbPath,
    const std::unordered_map<std::string, int64_t> &Symbols,
    const std::string &PePath) {
  llvm::BumpPtrAllocator Allocator;
  llvm::pdb::PDBFileBuilder Builder(Allocator);

  if (auto EC = Builder.initialize(4096)) {
    llvm::errs() << "Failed to initialize PDB: "
                 << llvm::toString(std::move(EC)) << "\n";
    return false;
  }

  for (int i = 0; i < (int)llvm::pdb::kSpecialStreamCount; ++i) {
    if (Builder.getMsfBuilder().addStream(0).takeError()) {
      return false;
    }
  }

  auto &InfoBuilder = Builder.getInfoBuilder();
  InfoBuilder.setVersion(llvm::pdb::PdbRaw_ImplVer::PdbImplVC70);
  llvm::codeview::GUID FakeGUID;
  uint32_t FakeAge;
  std::tie(FakeGUID, FakeAge) = extractGUIDAndAge(PePath);
  InfoBuilder.setGuid(FakeGUID);
  InfoBuilder.setAge(FakeAge);
  InfoBuilder.setHashPDBContentsToGUID(false);

  auto &DbiBuilder = Builder.getDbiBuilder();
  DbiBuilder.setVersionHeader(llvm::pdb::PdbDbiV70);
  DbiBuilder.setMachineType(llvm::pdb::PDB_Machine::x86);
  DbiBuilder.setFlags(llvm::pdb::DbiFlags::FlagHasCTypesMask);
  DbiBuilder.setAge(FakeAge);
  DbiBuilder.setBuildNumber(14, 11);

  Builder.getTpiBuilder().setVersionHeader(llvm::pdb::PdbTpiV80);
  Builder.getIpiBuilder().setVersionHeader(llvm::pdb::PdbTpiV80);

  auto &GsiBuilder = Builder.getGsiBuilder();
  std::vector<llvm::pdb::BulkPublic> Publics;

  for (const auto &[Name, Offset] : Symbols) {
    llvm::pdb::BulkPublic Sym;
    Sym.Name = Name.c_str();
    Sym.NameLen = Name.size();
    Sym.Segment = 0;
    Sym.Offset = Offset;
    Sym.setFlags(llvm::codeview::PublicSymFlags::Function |
                 llvm::codeview::PublicSymFlags::Code);
    Publics.push_back(Sym);
  }

  if (!Publics.empty()) {
    std::sort(Publics.begin(), Publics.end(),
              [](auto &L, auto &R) { return std::strcmp(L.Name, R.Name) < 0; });
    GsiBuilder.addPublicSymbols(std::move(Publics));
  }

  auto GuidForCommit = InfoBuilder.getGuid();
  if (auto EC = Builder.commit(OutputPdbPath, &GuidForCommit)) {
    llvm::errs() << "Failed to commit PDB: " << llvm::toString(std::move(EC))
                 << "\n";
    return false;
  }

  llvm::outs() << "PDB generated: " << OutputPdbPath << "\n";

  return true;
}
