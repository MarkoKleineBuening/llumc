//
// Created by marko on 03.07.17.
//

#ifndef LLUMC_ENCODER_H
#define LLUMC_ENCODER_H

#include <iostream>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <sstream>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstIterator.h>
#include <llbmc/CallGraph/FunctionMatcher.h>
#include "InstructionEncoderLLUMC.h"

class Encoder {
public:

    Encoder(llvm::Module &module);

   void createMap(llvm::StringRef string);

    llvm::Instruction* getInstruction(int);

    void encodeFormula(llvm::StringRef string);

private:
    std::map<int, llvm::Instruction *> m_idMap;
    llvm::Module &m_module;
    InstructionEncoderLLUMC m_instructionEncoder;

    void encodeBasicBlock(llvm::BasicBlock &block);

    void encodeBasicBlockOverTerminator(llvm::BasicBlock &bb);

    void handleInstruction(llvm::Instruction &instruction);

    void handleCallNode(llvm::CallInst *pInst);
};


#endif //LLUMC_ENCODER_H
