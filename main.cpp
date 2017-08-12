#include <iostream>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <sstream>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstIterator.h>
#include "Encoder.h"
#include <llbmc/SMT/SMTLIB.h>
#include <llbmc/Solver/SMTContext.h>
#include <llbmc/Output/Common.h>
#include <llbmc/SMT/STP.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/FileSystem.h>

int number = 0;

enum SMTSolver {
    SMTLIB,
    STP,
    Boolector
};

llvm::Module *getModule(llvm::LLVMContext &llvmContext, std::string &fileName);

llvm::raw_ostream *ostream2();

void removeOldFiles();

int main(int argc, char *argv[]) {
    removeOldFiles();
    std::string fileName = "/home/marko/workspace/bitcodeModule.bc";
    //if (argc > 1) { fileName = argv[1]; }
    llvm::LLVMContext context;
    llvm::Module *module = getModule(context, fileName);

    SMTSolver smtSolver = STP;
    SMT::Solver *solver;
    SMT::SatCore *satCore;
    if (smtSolver = SMTLIB) {
        SMT::SMTLIB::Common *common = new SMT::SMTLIB::Common(*ostream2());
        solver = new SMT::SMTLIB(common);
    } else if (smtSolver = STP) {
        solver = new SMT::STP(SMT::STP::MiniSat, false);
        satCore = solver->getSatCore();
        solver->disableSimplifications();
        solver->outputCNF();
        solver->useSimpleCNF();
    } else if (smtSolver = Boolector) {

    }
    llbmc::SMTContext *smtContext = new llbmc::SMTContext(solver, 4);
    Encoder *enc = new Encoder(*module, smtContext);

    int numFunc = module->getFunctionList().size();
    std::cout << "Num Functions: " << numFunc << "\n";
    SMT::BoolExp *transitionExp;
    SMT::BoolExp *initialExp;
    SMT::BoolExp *goalExp;
    SMT::BoolExp *universalExp;
    for (auto &func: module->getFunctionList()) {
        if (func.size() < 1) { continue; }
        llvm::outs() << "Calculate State:";
        enc->calculateState(func.getName());
        llvm::outs() << "-------------------------------" << "\n";
        llvm::outs() << "Encoding: " << "\n";
        llvm::outs() << "Transition: " << "\n";
        transitionExp = enc->encodeFormula(func.getName());
        llvm::outs() << "Initial: ";
        initialExp = enc->getInitialExp();
        llvm::outs() << "...finished " << "\n";
        llvm::outs() << "Goal: ";
        goalExp = enc->getGoalExp();
        llvm::outs() << "...finished " << "\n";
        llvm::outs() << "Universal: ";
        universalExp = enc->getUniversalExp();
        llvm::outs() << "...finished " << "\n";

    }
    llvm::outs() << "\n";
    solver->assertConstraint(transitionExp);
    solver->solve();
    solver->pop();
    solver->assertConstraint(initialExp);
    solver->solve();
    solver->pop();
    solver->assertConstraint(goalExp);
    solver->solve();
    solver->pop();
    solver->assertConstraint(universalExp);
    solver->solve();

    llvm::outs() << "\n";

    std::cout << "Desc:" << solver->getDescription() << "\n";
    switch (solver->getResult()) {
        case SMT::Solver::Result::Satisfiable: {
            std::cout << "Result:" << "Satisfiable" << "\n";
            break;
        }
        case SMT::Solver::Result::Unsatisfiable: {
            std::cout << "Result:" << "Unsatisfiable" << "\n";
            break;
        }
        case SMT::Solver::Result::Unknown: {
            std::cout << "Result:" << "Unknown" << "\n";
            break;
        }
        default:
            std::cout << "Result:" << "ERROR DEFAULT" << "\n";


    }
    return 0;
}

void removeOldFiles() {
    llvm::outs()<<"Following files have been deleted: ";
    if( remove( "AIGtoCNF.txt" ) == 0 ){
        llvm::outs()<< "AIGtoCNF.txt, ";
    }
    if( remove( "NameToAIG.txt" ) == 0 ){
        llvm::outs()<< "NameToAIG.txt, ";
    }
    if( remove( "output_0.cnf" ) == 0 ){
        llvm::outs()<< "output_0.cnf, ";
    }
    if( remove( "output_1.cnf" ) == 0 ){
        llvm::outs()<< "output_1.cnf, ";
    }
    if( remove( "output_2.cnf" ) == 0 ){
        llvm::outs()<< "output_2.cnf, ";
    }
    if( remove( "output_3.cnf" ) == 0 ){
        llvm::outs()<< "output_3.cnf";
    }
    llvm::outs() << ".\n";

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
    std::cout << "Module loaded from " << fileName << std::endl;
    return module;
}

llvm::raw_ostream *ostream2() {
    llvm::StringRef sRefName("/home/marko/test");
    std::error_code err_code;
    llvm::raw_fd_ostream *raw = new llvm::raw_fd_ostream(sRefName, err_code, llvm::sys::fs::OpenFlags::F_Append);
    return raw;
}


