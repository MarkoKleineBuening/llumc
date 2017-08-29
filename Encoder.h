//
// Created by marko on 03.07.17.
//

#ifndef LLUMC_ENCODER_H
#define LLUMC_ENCODER_H

#include <set>
#include <iostream>
#include <sstream>

#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstIterator.h>

#include <llbmc/CallGraph/FunctionMatcher.h>
#include <llbmc/Solver/SMTContext.h>

#include "InstructionEncoderLLUMC.h"
#include "IObserver.h"


class Encoder : public IObserver {
public:
    Encoder(llvm::Module &module, llbmc::SMTContext *context);

    SMT::BoolExp * encodeFormula(llvm::StringRef string);

    void calculateState(llvm::StringRef string);

    void Update(SMT::BoolExp *expr);

    SMT::BoolExp *getInitialExp();

    SMT::BoolExp *getGoalExp();

    SMT::BoolExp *getUniversalExp();

    SMT::BoolExp *getCompleteTest();

    SMT::BoolExp *getBreaker();

    SMT::BoolExp *getBreaker1();

    SMT::BoolExp *getBreaker2();

private:
    llvm::Module &m_module;
    SMTTranslator m_SMTTranslator;
    llbmc::SMTContext *m_smtContext;
    InstructionEncoderLLUMC m_instructionEncoder;

    std::map<llvm::StringRef, int> m_bbmap;
    std::map<llvm::StringRef, int> m_variableMap;
    std::set<llvm::StringRef> m_variableSet;

    SMT::BoolExp *m_currentFormula;

    SMT::BoolExp * encodeBasicBlockOverTerminator(llvm::BasicBlock &bb, int width);

    void handleInstruction(llvm::Instruction &instruction);

    void handleCallNode(llvm::CallInst *pInst);

    bool isOnlyUsedInBasicBlock(llvm::StringRef string, llvm::BasicBlock *BB, llvm::Value *user);

    bool isUsedInBasicBlock(llvm::StringRef string, llvm::BasicBlock *currentBB, llvm::Value *user);

    SMT::BoolExp *same();


    SMT::BoolExp *encodeSingleBlock(int width, std::string name);
};


#endif //LLUMC_ENCODER_H
