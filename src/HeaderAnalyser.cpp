#include "HeaderAnalyser.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/Path.h"

#include <llvm/DebugInfo/CodeView/SymbolSerializer.h>
#include <llvm/DebugInfo/MSF/MSFBuilder.h>
#include <llvm/DebugInfo/PDB/Native/DbiModuleDescriptorBuilder.h>
#include <llvm/DebugInfo/PDB/Native/DbiStreamBuilder.h>
#include <llvm/DebugInfo/PDB/Native/InfoStreamBuilder.h>
#include <llvm/DebugInfo/PDB/Native/RawTypes.h>

#include <mutex>
#include <windows.h>

#include <fstream>
#include <unordered_map>
#include <unordered_set>

#include "pl/SymbolProvider.h"

using namespace clang;
using namespace clang::tooling;

static std::unordered_set<std::string> includedFiles;
static std::unordered_set<std::string> mangledNames;
static std::unordered_set<std::string> externMangledNames;
static std::mutex mapMutex;

class FunctionVisitor : public RecursiveASTVisitor<FunctionVisitor> {
public:
  explicit FunctionVisitor(ASTContext *context) : context(context) {}
  bool VisitFunctionDecl(FunctionDecl *funcDecl) {
    const SourceManager &SM = context->getSourceManager();
    llvm::SmallVector<char, 128> filePath;
    llvm::StringRef filePathRef = SM.getFilename(funcDecl->getLocation());
    filePath.append(filePathRef.begin(), filePathRef.end());

    llvm::sys::path::remove_dots(filePath, true);
    llvm::sys::path::native(filePath);

    if (auto *parent = dyn_cast<CXXRecordDecl>(funcDecl->getParent())) {
      if (parent->getDescribedClassTemplate() || parent->isDependentType()) {
        return true;
      }
    }

    else if (funcDecl->getTemplatedKind() != FunctionDecl::TK_NonTemplate ||
             funcDecl->isTemplateInstantiation()) {
      return true;
    }

    llvm::StringRef funcName = funcDecl->getName();
    if (funcName.starts_with("$")) {
      return true;
    }

    std::string filePathStr(filePath.begin(), filePath.end());
    if (includedFiles.find(filePathStr) != includedFiles.end()) {
      auto mangleContext =
          MicrosoftMangleContext::create(*context, context->getDiagnostics());
      if (mangleContext->shouldMangleDeclName(funcDecl)) {
        std::string mangledName;
        llvm::raw_string_ostream mangledNameStream(mangledName);
        mangleContext->mangleName(funcDecl, mangledNameStream);
        mangledNameStream.flush();

        std::lock_guard<std::mutex> lock(mapMutex);
        mangledNames.emplace(mangledName);
      }
    }
    return true;
  }

private:
  ASTContext *context;
};

class FunctionConsumer : public ASTConsumer {
public:
  explicit FunctionConsumer(ASTContext *context) : visitor(context) {}

  void HandleTranslationUnit(ASTContext &context) override {
    visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  FunctionVisitor visitor;
};

class IncludeTrackingAction : public ASTFrontendAction {
public:
  void ExecuteAction() override {
    Preprocessor &PP = getCompilerInstance().getPreprocessor();
    const SourceManager &SM = getCompilerInstance().getSourceManager();

    PP.addPPCallbacks(std::make_unique<MyPPCallbacks>(SM));

    ASTFrontendAction::ExecuteAction();
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override {
    return std::make_unique<FunctionConsumer>(&CI.getASTContext());
  }

private:
  class MyPPCallbacks : public clang::PPCallbacks {
  public:
    explicit MyPPCallbacks(const SourceManager &SM) : SM(SM) {}

    void InclusionDirective(SourceLocation HashLoc, const Token &IncludeTok,
                            StringRef FileName, bool IsAngled,
                            CharSourceRange FilenameRange,
                            OptionalFileEntryRef File, StringRef SearchPath,
                            StringRef RelativePath, const Module *Imported,
                            SrcMgr::CharacteristicKind FileType) override {
      if (File) {
        std::string filePathStr = File->getName().str();
        llvm::SmallVector<char, 128> filePath(filePathStr.begin(),
                                              filePathStr.end());

        llvm::sys::path::remove_dots(filePath, true);
        llvm::sys::path::native(filePath);
        filePathStr = std::string(filePath.begin(), filePath.end());

        if (SM.isInMainFile(HashLoc)) {
          includedFiles.insert(filePathStr);
        }
      }
    }

  private:
    const SourceManager &SM;
  };
};

std::unordered_map<std::string, int64_t>
HeaderAnalyser::analyze(const std::string &buildPath,
                        const std::string &sourceFile,
                        const std::string &externalSymbolList) {
  {
    std::lock_guard<std::mutex> lock(mapMutex);
    includedFiles.clear();
    mangledNames.clear();
  }

  if (!externalSymbolList.empty()) {
    std::ifstream file(externalSymbolList);
    if (file.is_open()) {
      std::string line;
      while (std::getline(file, line)) {
        externMangledNames.emplace(line);
      }
    }
  }

  std::string errorMessage;
  auto compilationDatabase =
      CompilationDatabase::loadFromDirectory(buildPath, errorMessage);
  if (!compilationDatabase) {
    llvm::errs() << "Error loading compilation database from " << buildPath
                 << ": " << errorMessage << "\n";
    return {};
  }

  ClangTool tool(*compilationDatabase, {sourceFile});
  tool.run(newFrontendActionFactory<IncludeTrackingAction>().get());

  uintptr_t moduleBase = 0;
  HMODULE hModule = GetModuleHandle(nullptr);
  if (hModule == nullptr) {
    llvm::errs()
        << "Error: Failed to get base address of the current program.\n";
    return {};
  } else {
    moduleBase = reinterpret_cast<uintptr_t>(hModule);
  }

  std::unordered_map<std::string, int64_t> rvaMap;
  {
    for (const auto &mangledName : mangledNames) {
      auto res = pl::symbol_provider::pl_resolve_symbol_silent_n(
          mangledName.data(), mangledName.size());
      if (res) {
        rvaMap[mangledName] = reinterpret_cast<uintptr_t>(res) - moduleBase;
      }
    }
  }

  llvm::outs() << "Found " << rvaMap.size() << " symbols\n";
  int counts = rvaMap.size();
  {
    for (const auto &mangledName : externMangledNames) {
      auto res = pl::symbol_provider::pl_resolve_symbol_silent_n(
          mangledName.data(), mangledName.size());
      if (res) {
        rvaMap[mangledName] = reinterpret_cast<uintptr_t>(res) - moduleBase;
      }
    }
  }
  llvm::outs() << "Found " << rvaMap.size() - counts << " old symbols\n";

  return rvaMap;
}