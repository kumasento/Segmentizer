/** ExtractCodeSegment.cc
 *
 * Extract the code segments enclosed by #pragma code_segment.
 */

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

namespace {

// Hold the segments enclosed by the start_segment/end_segment
struct CodeSegmentLoc {
  int64_t line, offset;

  CodeSegmentLoc() : line(-1), offset(-1) {}
  CodeSegmentLoc(int64_t line, int64_t offset) : line(line), offset(offset) {}
};

struct CodeSegment {
  FileID fid;
  CodeSegmentLoc start, end;
  bool closed;

  CodeSegment(FileID fid) : closed(false), fid(fid) {}
};

class CodeSegmentList {
  std::vector<CodeSegment> segments;

public:
  const std::vector<CodeSegment> &data() { return segments; }

  void startSegment(SourceManager &SM, SourceLocation sloc) {
    assert((segments.empty() || segments.back().closed) &&
           "The last segment should be properly closed.");
    int64_t line, offset;
    std::tie(line, offset) = getLineAndOffset(SM, sloc);

    CodeSegment cs(SM.getFileID(sloc));
    cs.start = {line, offset};

    segments.push_back(cs);
  }

  void endSegment(SourceManager &SM, SourceLocation sloc) {
    assert((!segments.empty() && !segments.back().closed) &&
           "The last segment should be unclosed.");

    int64_t line, offset;
    std::tie(line, offset) = getLineAndOffset(SM, sloc, /*start=*/false);

    segments.back().end = {line, offset};
    segments.back().closed = true;
  }

private:
  std::pair<int64_t, int64_t>
  getLineAndOffset(SourceManager &SM, SourceLocation sloc, bool start = true) {
    int64_t line = SM.getExpansionLineNumber(sloc);
    if (!start)
      line++;

    SourceLocation firstCol = SM.translateLineCol(SM.getFileID(sloc), line, 1);
    int64_t offset = SM.getFileOffset(firstCol);

    return {line, offset};
  }
};

class StartCodeSegmentPragmaHandler : public PragmaHandler {
  CodeSegmentList &segs;

public:
  StartCodeSegmentPragmaHandler(CodeSegmentList &segs)
      : PragmaHandler("start_segment"), segs(segs) {}

  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &PragmaTok) {
    llvm::errs() << "Handling start_segment ...\n";

    SourceManager &SM = PP.getSourceManager();
    SourceLocation sloc = PragmaTok.getLocation();

    segs.startSegment(SM, sloc);
  }
};

class EndCodeSegmentPragmaHandler : public PragmaHandler {
  CodeSegmentList &segs;

public:
  EndCodeSegmentPragmaHandler(CodeSegmentList &segs)
      : PragmaHandler("end_segment"), segs(segs) {}

  void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                    Token &PragmaTok) {
    llvm::errs() << "Handling end_segment ...\n";

    SourceManager &SM = PP.getSourceManager();
    SourceLocation sloc = PragmaTok.getLocation();

    segs.endSegment(SM, sloc);
  }
};

class ExtractCodeSegmentConsumer : public ASTConsumer {
  CompilerInstance &CI;
  CodeSegmentList &segs;

public:
  ExtractCodeSegmentConsumer(CompilerInstance &CI, CodeSegmentList &segs)
      : CI(CI), segs(segs) {}

  bool HandleTopLevelDecl(DeclGroupRef DG) override {
    SourceManager &SM = CI.getSourceManager();

    for (auto it : llvm::enumerate(segs.data())) {
      const CodeSegment &seg = it.value();
      const FileEntry *F = SM.getFileEntryForID(seg.fid);
      StringRef buf = SM.getBufferData(seg.fid);

      llvm::outs() << F->getName() << "\n";
      llvm::outs() << buf.substr(seg.start.offset,
                                 seg.end.offset - seg.start.offset);
    }

    return true;
  }
};

class ExtractCodeSegmentAction : public PluginASTAction {
  CodeSegmentList segs;

public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef) override {
    clang::Preprocessor &PP = CI.getPreprocessor();

    PP.AddPragmaHandler(new StartCodeSegmentPragmaHandler(segs));
    PP.AddPragmaHandler(new EndCodeSegmentPragmaHandler(segs));

    return std::make_unique<ExtractCodeSegmentConsumer>(CI, segs);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }
};
} // namespace

static FrontendPluginRegistry::Add<ExtractCodeSegmentAction>
    X("extract-code-segment",
      "Extract code segments wrapped under #pragma code_segment");
