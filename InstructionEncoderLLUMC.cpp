//
// Created by marko on 04.07.17.
//

#include "InstructionEncoderLLUMC.h"


void InstructionEncoderLLUMC::visitSpecification(llvm::CallInst &I)
{
    llvm::StringRef name = llbmc::FunctionMatcher::getCalledFunction(I)->getName();
    std::cout << "Visit Specification" << std::endl;
}