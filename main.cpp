#include <iostream>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <sstream>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstIterator.h>
#include "Encoder.h"

int number = 0;

llvm::Module *getModule(llvm::LLVMContext &llvmContext, std::string &fileName);

void encodeFormula(llvm::Module *module);

void encodeFunction(llvm::Function &func, llvm::Module *module);

void encodeBasicBlock(llvm::BasicBlock &block, llvm::Module *pModule);

int main(int argc, char *argv[]) {
    std::string fileName = "/home/marko/workspace/bitcodeModule.bc";
    if (argc > 1) { fileName = argv[1]; }
    llvm::LLVMContext context;
    llvm::Module *module = getModule(context, fileName);

    //Encoder enc(*module);
    Encoder *enc = new Encoder(*module);
    //enc->createMap();

    int numFunc = module->getFunctionList().size();
    std::cout << "Num Functions: " << numFunc << "\n";

    for (auto &func: module->getFunctionList()) {
        if (func.size() < 1) { continue; }
        std::cout << "Function started." << "\n";
        enc->createMap(func.getName());
        std::cout << std::endl << "-------------------------------" << std::endl;
        std::cout << "Encoding started." << "\n";
        enc->encodeFormula(func.getName());

    }

    return 0;



    int numBB = 0;
    int numInst = 0;
    int numPhi = 0;
    int numCall = 0;
    //Iterate over Functions
    for (llvm::Module::FunctionListType::iterator it = module->getFunctionList().begin(), end = module->getFunctionList().end();
         it != end; ++it) {
        if (it.operator*().size() < 1) { continue; }
        numBB = 0;
        numInst = 0;
        numPhi = 0;
        numCall = 0;
        std::vector<std::string> phiVector;
        std::vector<std::string> bbVector;

        std::cout << "\n-------- Function-------- \n";
        numBB = it.operator*().size();
        std::cout << "Number of BasicBlocks: " << numBB << "\n";

        //Iterate over BasicBlocks
        for (llvm::Function::iterator itF = it.operator*().begin(), end = it.operator*().end(); itF != end; ++itF) {
            std::cout << " \n";
            numInst = itF.operator*().size();
            std::string nameBB = itF.operator*().getName();
            std::cout << "BasicBlock (name, size): (" << nameBB << ", " << numInst << ")\n";
            bbVector.push_back(nameBB);


            llvm::TerminatorInst *terminatorInst = itF.operator*().getTerminator();

            //Iterate over Instructions
            for (llvm::BasicBlock::iterator itbb = itF.operator*().begin(), end = itF.operator*().end();
                 itbb != end; ++itbb) {
                llvm::StringRef x = itbb.operator*().getValueName()->first();

                std::cout << "Instructions: " << itbb.operator*().getOpcodeName() << ": ";


                if (llvm::PHINode *P = llvm::dyn_cast<llvm::PHINode>(&itbb.operator*())) {
                    //handle Phi Node
                    //std::cout << "PhiNode";
                    for (int i = 0; i < P->getNumOperands(); i++) {
                        if (P->getOperand(i)->getName().empty() || P->getOperand(i)->getName().size() < 1) {
                            std::string name = "x";
                            P->getOperand(i)->setName(name);
                        }
                        if (!(std::find(phiVector.begin(), phiVector.end(), P->getOperand(i)->getName()) !=
                              phiVector.end())) {
                            if (!P->getOperand(i)->getName().empty()) {
                                phiVector.push_back(P->getOperand(i)->getName());
                            }
                        }
                    }
                } else if (llvm::CallInst *C = llvm::dyn_cast<llvm::CallInst>(&itbb.operator*())) {
                    //handle Call Node
                    //std::cout << "CallNode";
                    //if call get's saved into a variable
                    //C->get
                    C->getValueName();
                    std::cout << "ValueName:" << C->getValueName() << ",  ";
                    numCall++;
                }

                //Iterate over Operands of Instruction
                for (int i = 0; i < itbb.operator*().getNumOperands(); i++) {

                    //information output
                    llvm::Value *v = itbb.operator*().getOperand(i);
                    std::string s = v->getName();
                    if (s.empty()) {
                        std::string name = "x";
                        v->setName(name);
                        s = v->getName();
                    }
                    if (!(i + 1 < itbb.operator*().getNumOperands())) {
                        std::cout << s;
                    } else {
                        std::cout << s << ", ";
                    }
                }

                /*
                    if(itbb.operator*().getName().size() < 1){
                        itbb.operator*().setName("x");
                    }
                    std::string normalName = itbb.operator*().getName();
                    std::string valueName = itbb.operator*().getValueName()->getKey();
                    std::cout << "\nnormal name:" << normalName << " value name:" << valueName << "\n";
                */
                std::cout << "\n";
            }
        }
        int stateSpaceSize = 0;
        numPhi = phiVector.size();
        stateSpaceSize += ceil(log(numBB) / log(2.0)) * 4;
        stateSpaceSize += numPhi * 2;
        stateSpaceSize += numCall * 2;
        std::cout << "\nNumBB: " << numBB << "  NumPhi: " << numPhi << "  NumCall: " << numCall << "\n";
        std::cout << "StateSpaceSize: " << stateSpaceSize << "\n";
    }
    encodeFormula(module);
    return 0;
}

void encodeFormula(llvm::Module *module) {
    for (llvm::Module::FunctionListType::iterator it = module->getFunctionList().begin(), end = module->getFunctionList().end();
         it != end; ++it) {
        std::cout << "\n-------- Function Encoding-------- \n";
        llvm::Function &func = it.operator*();
        encodeFunction(func, module);
    }
}

void encodeFunction(llvm::Function &function, llvm::Module *module) {
    number = 0;
    int numBB = function.size();
    std::cout << "Encoding of BasicBlocks: " << numBB << "\n";

    //Iterate over BasicBlocks
    for (llvm::Function::iterator itF = function.begin(), end = function.end(); itF != end; ++itF) {

        llvm::BasicBlock &bb = itF.operator*();
        encodeBasicBlock(bb, module);
    }
}

void encodeBasicBlock(llvm::BasicBlock &block, llvm::Module *pModule) {
    std::string nameBB = block.getName();
    std::cout << "\nBasic Block" << ++number << ":" << nameBB << "\n";
    llvm::TerminatorInst *termInst = block.getTerminator();
    std::cout << "Terminator Opcodename: " << termInst->getOpcodeName() << "\n";
    std::string firstOperandName = termInst->getOperand(0)->getName();
    std::cout << "First Operand: " << firstOperandName << "\n";
    std::cout << "Number of Uses: " << termInst->getOperand(0)->getNumUses() << "\n";

    llvm::Value *value = termInst->getOperand(0);
    std::cout << "termInst operand valueID:" << value->getValueID() << "\n";

    llvm::BasicBlock &BB = block;
    for (llvm::Instruction &I : BB) {
        std::map<int, llvm::Instruction *> idMap;
        idMap[I.getValueID()] = &I;

        std::cout << "valueID:" << I.getValueID() << "\n";
        if (I.getValueID() == value->getValueID()) {
            //eval I
            if (llvm::ICmpInst *ICmp = llvm::dyn_cast<llvm::ICmpInst>(&I)) {
                unsigned int numArgs = I.getNumOperands();
                for (unsigned int i = 0; i < numArgs; i++) {
                    std::cout << "    icmp operand(" << i << "):" << ICmp->getOperand(i)->getValueID();
                }
                std::cout << "\n";
            }

        }
    }

    /*
    for (llvm::Value::use_iterator it = termInst->getOperand(0)->uses().begin(), end = termInst->getOperand(
            0)->uses().end(); it != end; ++it) {

        std::string name = it->get()->getName();
        std::cout << "Use Name: " << name << "\n";

        llvm::User *user = it->getUser();
        llvm::Instruction *inst = (llvm::Instruction *) user;
        std::cout << "User Name: " << inst->getOpcodeName() << "\n";
    }
     */


}

llvm::Module *getModule(llvm::LLVMContext &llvmContext, std::string &fileName) {
    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> owningBuffer = llvm::MemoryBuffer::getFileOrSTDIN(fileName);
    llvm::MemoryBuffer *buffer = NULL;
    if (!owningBuffer.getError()) {
        buffer = owningBuffer->get();
    }

    if (buffer == NULL) {
        // no buffer -> no module, return an error
        throw std::invalid_argument("Could not open file.");

    }

    llvm::Module *module = NULL;
    std::string errorMsg;

    //3.7 and 4.0 or just one of them?
    llvm::ErrorOr<std::unique_ptr<llvm::Module>> moduleOrError = llvm::parseBitcodeFile(buffer->getMemBufferRef(),
                                                                                        llvmContext);
    std::error_code ec = moduleOrError.getError();
    if (ec) {
        errorMsg = ec.message();
    } else {
        module = moduleOrError->release();
    }

    if (module == NULL) {
        throw std::invalid_argument("Could not parse bitcode file.");
    }

    return module;
}




