#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_set>
#include <vector>

using namespace llvm;

namespace {
struct OurAggressiveDCEPass : public FunctionPass {
    static char ID;
    std::unordered_set<Instruction*> Live;
    std::vector<Instruction*> Worklist;

    OurAggressiveDCEPass() : FunctionPass(ID) {
    }

    bool isAlwaysLive(Instruction &I);
    void markLive(Instruction *I);
    void initializeLive(Function &F);
    void propagateLive();
    bool removeDeadInstructions(Function &F);

    bool runOnFunction(Function &F) override {
        Live.clear();
        Worklist.clear();
        initializeLive(F);
        propagateLive();
        return removeDeadInstructions(F);
    }

};

} // namespace

bool OurAggressiveDCEPass::isAlwaysLive(Instruction &I) {
    return I.isTerminator() || I.mayHaveSideEffects();
}

void OurAggressiveDCEPass::markLive(Instruction *I) {
    if (I == nullptr) {
        return;
    }
    if (Live.find(I) != Live.end()) {
        return;
    }
    Live.insert(I);
    Worklist.push_back(I);
}

void OurAggressiveDCEPass::initializeLive(Function &F) {
    for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
            if (isAlwaysLive(I)) {
                markLive(&I);
            }
        }
    }
}

void OurAggressiveDCEPass::propagateLive() {
    while (!Worklist.empty()) {
        Instruction *I = Worklist.back();
        Worklist.pop_back();

        for (Use &U : I->operands()) {
            if (Instruction *Op = dyn_cast<Instruction>(U.get())) {
                markLive(Op);
            }
        }
    }
}

bool OurAggressiveDCEPass::removeDeadInstructions(Function &F) {
    std::vector<Instruction*> InstructionsToRemove;

    for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
            if (Live.find(&I) == Live.end()) {
                InstructionsToRemove.push_back(&I);
            }
        }
    }

    for (Instruction *I : InstructionsToRemove) {
        I->replaceAllUsesWith(UndefValue::get(I->getType()));
    }
    for (Instruction *I : InstructionsToRemove) {
        I->eraseFromParent();
    }

    return !InstructionsToRemove.empty();
}

char OurAggressiveDCEPass::ID = 0;
static RegisterPass<OurAggressiveDCEPass> X("adcep", "Our aggressive dead code elimination",
                                false, false);
