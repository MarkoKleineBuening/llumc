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


class InstructionEncoderLLUMC : public llvm::LLBMCVisitor<InstructionEncoderLLUMC>{
        //public llvm::InstVisitor<InstructionEncoderLLUMC>{
public:
    void visitSpecification(llvm::CallInst &I);


};


#endif //LLUMC_INSTRUCTIONENCODER_H
