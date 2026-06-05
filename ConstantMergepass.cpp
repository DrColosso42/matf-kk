#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <utility>
#include <vector>

using namespace llvm;

namespace {
struct OurConstantMergePass : public ModulePass {
    static char ID;
    OurConstantMergePass() : ModulePass(ID) {
    }

    bool isMergeCandidate(GlobalVariable &GV);
    bool runOnModule(Module &M) override;

};

} // namespace

bool OurConstantMergePass::isMergeCandidate(GlobalVariable &GV) {
    return GV.isConstant()
        && GV.hasInitializer()
        && !GV.isDeclaration()
        && GV.hasGlobalUnnamedAddr();
}

bool OurConstantMergePass::runOnModule(Module &M) {
    std::map<std::pair<Type*, Constant*>, GlobalVariable*> Seen;
    std::vector<std::pair<GlobalVariable*, GlobalVariable*>> ToMerge;

    for (GlobalVariable &GV : M.globals()) {
        if (!isMergeCandidate(GV)) {
            continue;
        }

        auto Key = std::make_pair(GV.getValueType(), GV.getInitializer());
        auto It = Seen.find(Key);
        if (It == Seen.end()) {
            Seen[Key] = &GV;
        } else {
            ToMerge.push_back(std::make_pair(&GV, It->second));
        }
    }

    for (auto &P : ToMerge) {
        GlobalVariable *Duplicate = P.first;
        GlobalVariable *Canonical = P.second;

        Duplicate->replaceAllUsesWith(Canonical);
        Duplicate->eraseFromParent();
    }

    return !ToMerge.empty();
}

char OurConstantMergePass::ID = 0;
static RegisterPass<OurConstantMergePass> X("ocm", "Our constant merge",
                                false, false);
