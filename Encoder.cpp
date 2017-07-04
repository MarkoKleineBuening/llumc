//
// Created by marko on 03.07.17.
//

#include "Encoder.h"



Encoder::Encoder(llvm::Module &pModule) : m_idMap(), m_module(pModule) {

}

std::map<int, llvm::Instruction *> Encoder::createMap(llvm::Function &func){
    std::map<int, llvm::Instruction *> idMap;
    for (llvm::BasicBlock &BB : func) {
        for (llvm::Instruction &I : BB) {
            idMap[I.getValueID()] = &I;
        }
    }
}

llvm::Instruction *Encoder::getInstruction(int) {
    return nullptr;
}


