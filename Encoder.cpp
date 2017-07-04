//
// Created by marko on 03.07.17.
//

#include "Encoder.h"


Encoder::Encoder(llvm::Module &module) : m_idMap(), m_module(module), m_instructionEncoder() {

}

void Encoder::createMap(llvm::StringRef string) {
    std::cout << "Map Started." << std::endl;
    llvm::Function *func = m_module.getFunction(string);
    for (llvm::BasicBlock &BB : *func) {
        for (llvm::Instruction &I : BB) {
            //std::cout << "Instruction:" << I.getOpcodeName() << "/" << I.getValueID() << "/" << std::endl;
            m_idMap[I.getValueID()] = &I;
        }
    }
}

llvm::Instruction *Encoder::getInstruction(int valueID) {
    return m_idMap.at(valueID);
}

void Encoder::encodeFormula(llvm::StringRef string) {

    llvm::Function *Func = m_module.getFunction(string);
    int numBB = Func->size();
    std::cout << "Encoding of BasicBlocks: " << numBB << "\n";
    for (llvm::BasicBlock &BB : *Func) {
        encodeBasicBlock(BB);
    }
}

void Encoder::encodeBasicBlock(llvm::BasicBlock &BB) {
    std::string str = BB.getName();
    std::cout << "BB:" << str << std::endl;
    for (llvm::Instruction &I : BB) {
        handleInstruction(I);
    }
    std::cout << std::endl << "-------------------------------" << std::endl;
}

void Encoder::handleInstruction(llvm::Instruction &I) {
    if (llvm::CallInst *C = llvm::dyn_cast<llvm::CallInst>(&I)) {
        handleCallNode(C);
    } else if (llvm::ICmpInst *ICmp = llvm::dyn_cast<llvm::ICmpInst>(&I)) {
        std::cout << "ICmpNode" << std::endl;

    } else if (llvm::PHINode *P = llvm::dyn_cast<llvm::PHINode>(&I)) {
        std::cout << "PHINode" << std::endl;
    } else {
        std::cout << "nothing from above" << std::endl;
    }
}

void Encoder::handleCallNode(llvm::CallInst *C) {
    assert(C->getValueID()==68);
    if (llbmc::FunctionMatcher::isCalloc(*C)){
        std::cout << "isCalloc" << std::endl;
    }else if (llbmc::FunctionMatcher::isFree(*C)){
        std::cout << "isFree" << std::endl;
    }else if(llbmc::FunctionMatcher::isSpecification(*C)){
        std::cout << "isSpecification" << std::endl;
        m_instructionEncoder.visit(C);
    }else if(llbmc::FunctionMatcher::isAssert(*C)){
        std::cout << "isAssert" << std::endl;
    }else if(llbmc::FunctionMatcher::isAssertFail(*C)){
        std::cout << "isAssertFail" << std::endl;
    }else{
        std::cout << "Undefined Call" << std::endl;
    }
}

void Encoder::encodeBasicBlockOverTerminator(llvm::BasicBlock &bb) {
    std::string str = bb.getName();
    std::cout << "BB:" << str << std::endl;
    llvm::TerminatorInst *termInst = bb.getTerminator();
    int termID = termInst->getValueID();
    std::cout << "termID:" << termID << std::endl;
    for (int i = 0; i < termInst->getNumOperands(); i++) {
        llvm::Instruction *inst = (llvm::Instruction *) termInst->getOperand(i);


        //llvm::Instruction *inst = m_idMap.at(valueID);
    }
    std::cout << std::endl << "-------------------------------" << std::endl;
}






