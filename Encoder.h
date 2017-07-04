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

class Encoder {
public:

    Encoder(llvm::Module &pModule);

    std::map<int, llvm::Instruction *> createMap(llvm::Function &func);

    llvm::Instruction* getInstruction(int);

private:
    std::map<int, llvm::Instruction *> m_idMap;
    llvm::Module m_module;

};


#endif //LLUMC_ENCODER_H
