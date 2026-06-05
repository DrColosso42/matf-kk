#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>

using namespace llvm;

namespace {
struct OurSimplifyCFGPass : public FunctionPass {
    static char ID;
    OurSimplifyCFGPass() : FunctionPass(ID) {
    }

    bool foldConstantBranches(Function &F);
    bool removeUnreachableBlocks(Function &F);
    bool mergeIntoPredecessor(Function &F);

    bool runOnFunction(Function &F) override {
        bool Changed = false;
        bool LocalChange = true;

        while (LocalChange) {
            LocalChange = false;
            LocalChange |= foldConstantBranches(F);
            LocalChange |= removeUnreachableBlocks(F);
            LocalChange |= mergeIntoPredecessor(F);
            Changed |= LocalChange;
        }

        return Changed;
    }

};

} // namespace

bool OurSimplifyCFGPass::foldConstantBranches(Function &F) {
    std::vector<BranchInst*> ToFold;

    for (BasicBlock &BB : F) {
        if (BranchInst *Br = dyn_cast<BranchInst>(BB.getTerminator())) {
            if (Br->isConditional() && isa<ConstantInt>(Br->getCondition())) {
                ToFold.push_back(Br);
            }
        }
    }

    for (BranchInst *Br : ToFold) {
        ConstantInt *Cond = cast<ConstantInt>(Br->getCondition());
        BasicBlock *Taken = Cond->isOne() ? Br->getSuccessor(0) : Br->getSuccessor(1);
        BasicBlock *NotTaken = Cond->isOne() ? Br->getSuccessor(1) : Br->getSuccessor(0);

        NotTaken->removePredecessor(Br->getParent());
        BranchInst::Create(Taken, Br->getParent());
        Br->eraseFromParent();
    }

    return !ToFold.empty();
}

bool OurSimplifyCFGPass::removeUnreachableBlocks(Function &F) {
    std::vector<BasicBlock*> Dead;
    bool First = true;

    for (BasicBlock &BB : F) {
        if (First) {
            First = false;
            continue;
        }
        if (pred_empty(&BB)) {
            Dead.push_back(&BB);
        }
    }

    for (BasicBlock *BB : Dead) {
        for (BasicBlock *Succ : successors(BB)) {
            Succ->removePredecessor(BB);
        }
        while (!BB->empty()) {
            Instruction &I = BB->back();
            I.replaceAllUsesWith(UndefValue::get(I.getType()));
            I.eraseFromParent();
        }
        BB->eraseFromParent();
    }

    return !Dead.empty();
}

bool OurSimplifyCFGPass::mergeIntoPredecessor(Function &F) {
    BasicBlock *Target = nullptr;

    for (BasicBlock &BB : F) {
        BasicBlock *Pred = BB.getSinglePredecessor();
        if (Pred == nullptr || Pred == &BB) {
            continue;
        }
        BranchInst *Br = dyn_cast<BranchInst>(Pred->getTerminator());
        if (Br == nullptr || !Br->isUnconditional()) {
            continue;
        }
        Target = &BB;
        break;
    }

    if (Target == nullptr) {
        return false;
    }

    BasicBlock *Pred = Target->getSinglePredecessor();
    Instruction *Br = Pred->getTerminator();

    while (PHINode *Phi = dyn_cast<PHINode>(&Target->front())) {
        Phi->replaceAllUsesWith(Phi->getIncomingValue(0));
        Phi->eraseFromParent();
    }

    while (!Target->empty()) {
        Instruction &I = Target->front();
        I.moveBefore(Br);
    }
    Br->eraseFromParent();
    Target->replaceAllUsesWith(Pred);
    Target->eraseFromParent();

    return true;
}

char OurSimplifyCFGPass::ID = 0;
static RegisterPass<OurSimplifyCFGPass> X("oscfg", "Our simplify CFG",
                                false, false);
