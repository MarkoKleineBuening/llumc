//
// Created by marko on 04.07.17.
//

#ifndef LLUMC_INSTRUCTIONENCODER_H
#define LLUMC_INSTRUCTIONENCODER_H

#include <iostream>
#include <llvm/IR/InstVisitor.h>
#include <llvm/IR/Instructions.h>
#include <llbmc/CallGraph/FunctionMatcher.h>
#include <llbmc/CallGraph/LLBMCVisitor.h>
#include <llbmc/Util/LLBMCException.h>
#include <llvm/Support/raw_ostream.h>
#include "SMTTranslator.h"
#include <llbmc/Solver/DefaultSMTTranslator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include "llvm/IR/Operator.h"
#include <llbmc/Solver/SMTContext.h>


class InstructionEncoderLLUMC : public llvm::LLBMCVisitor<InstructionEncoderLLUMC> {
public:

    InstructionEncoderLLUMC(llbmc::SMTContext *context);

    void visitNonIntrinsicCall(llvm::CallInst &I);

    void visitSpecification(llvm::CallInst &I);

    void visitICmpInst(llvm::ICmpInst &I);

    void visitBranchInst(llvm::BranchInst &I);

    void visitAdd(llvm::BinaryOperator &I);

    void visitSub(llvm::BinaryOperator &I);

    void visitZExtInst(llvm::ZExtInst &I);

    void visitSExtInst(llvm::SExtInst &I);

    void visitPHINode(llvm::PHINode &I);

    void visitOr(llvm::BinaryOperator &I);

    void visitAnd(llvm::BinaryOperator &I);

    void visitMul(llvm::BinaryOperator &I);

    void visitAShr(llvm::BinaryOperator &I);

    void visitLShr(llvm::BinaryOperator &I);

    void visitShl(llvm::BinaryOperator &I);

    void visitReturnInst(llvm::ReturnInst &I);

    void visitUnreachableInst(llvm::UnreachableInst &I);

    void visitTruncInst(llvm::TruncInst &I);

    void visitSRem(llvm::BinaryOperator &I);

    void visitURem(llvm::BinaryOperator &I);

    void visitReturn(llvm::TerminatorInst &I);

    void visitAssertFail(llvm::CallInst &I);

    void visitAssert(llvm::CallInst &I);

    void visitAssume(llvm::CallInst &inst);

    void setBBMap(std::map<llvm::StringRef, int> map);

    void setVariableList(std::set<llvm::StringRef> variableSet);

    llvm::SmallPtrSet<SMT::BoolExp*, 1> getFormulaSet();

    llvm::SmallPtrSet<SMT::BoolExp *, 1> getFormulaSetICMP();

    llvm::SmallPtrSet<SMT::BoolExp*, 1> getFormulaSetSave();

    llvm::SmallPtrSet<SMT::BoolExp*, 1> getFormulaSetOverflow();

    SMT::BoolExp* getAssumeBoe();

    SMT::BoolExp* getAssumeBoeDash();

    SMT::BoolExp* getOverflowCheckBoe();

    SMT::BoolExp* getOverflowCheckBoeDash();


    std::set<llvm::StringRef> getNotUsedVar();

    void resetFormulaSet();

    void resetNotUsedVar();


    void resetFormulaSetSave();

    llvm::SmallPtrSet<SMT::BoolExp *, 1> getNegAssumeCond();

    void visitError(llvm::CallInst &I);

    void visitVerifierAssume(llvm::CallInst &I);

private:
    SMTTranslator m_SMTTranslator;
    llbmc::SMTContext *m_smtContext;
    std::set<llvm::StringRef> m_variableSet;
    std::set<llvm::StringRef> m_notUsedVar;
    llvm::SmallPtrSet<SMT::BoolExp*, 1> m_formulaSet;
    llvm::SmallPtrSet<SMT::BoolExp*, 1> m_formulaSetICMP;
    llvm::SmallPtrSet<SMT::BoolExp*, 1> m_formulaSetSave;
    llvm::SmallPtrSet<SMT::BoolExp*, 1> m_formulaNegAssumeCond;
    llvm::SmallPtrSet<SMT::BoolExp*, 1> m_overFlowCheck;
    SMT::BVExp*m_formulaSetBV;
    std::map<llvm::StringRef, int> m_bbmap;
    SMT::BVExp *m_lastExp;
    std::set<llvm::StringRef> m_alreadyVisited;
    SMT::BoolExp *m_assumeBoe;
    SMT::BoolExp *m_assumeBoeDash;

    bool isNameBefore(llvm::BasicBlock &bb, llvm::Value *op, llvm::StringRef currentName);


    bool isVerifierCall(llvm::Instruction *pInstruction);

    void checkForOverflow(llvm::BinaryOperator &I, SMT::BVExp *bv1, SMT::BVExp *bv2, std::string operation);
};


#endif //LLUMC_INSTRUCTIONENCODER_H
