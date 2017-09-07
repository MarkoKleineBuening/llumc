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
#include <fstream>

int number = 0;

enum SMTSolver {
    SMTLIB,
    STP,
    Boolector
};

int highestVar;

llvm::Module *getModule(llvm::LLVMContext &llvmContext, std::string &fileName);

llvm::raw_ostream *ostream2();

void removeOldFiles();

void summarizeOutputFiles();

void readFile(std::string file, std::string type);

void renameVariables(std::map<std::string, std::vector<unsigned int>> mapDash);

void outputMap(std::string fileMap, std::map<std::string, std::vector<unsigned int>> dashMap);

int main(int argc, char *argv[]) {
    removeOldFiles();
    std::string fileName = "/home/marko/workspace/bitcodeModule.bc";
    if (argc > 1) { fileName = argv[1]; }
    std::cout << "Filename: " << fileName << "\n";
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
    SMT::BoolExp *testExp;
    SMT::BoolExp *testGoal;
    SMT::BoolExp *testIni;
    for (auto &func: module->getFunctionList()) {
        if (func.size() < 1) { continue; }
        llvm::outs() << "Func name: " << func.getName() << "\n";
        if(func.getName().find("main")==-1){
            llvm::outs() << "--Next--\n";
            continue;
        }
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
        /*llvm::outs() << "Test: ";
        testExp = enc->getCompleteTest();
        llvm::outs() << "...finished " << "\n";
        llvm::outs() << "TestIni: ";*/


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


    /*solver->assertConstraint(testExp);
    solver->solve();*/

    std::map<std::string, std::vector<unsigned int>> dashMap = solver->getDashMap();
    //std::cout << "Size of Vector: " << dashMap.size() << "\n";
    /*for (auto &entry : dashMap) {
        std::cout << entry.first << ": ";
        for (auto &sec : entry.second) {
            std::cout << sec << ", ";
        }
        std::cout << "\n";
    }
    llvm::outs() << "\n";*/

    std::cout << "Desc:" << solver->getDescription() << "\n";
    /*switch (solver->getResult()) {
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

    }*/

    //return 0;
    summarizeOutputFiles();

    renameVariables(dashMap);

    bool outputMapBool = true;
    if(outputMapBool){
        std::string fileMap = "/home/marko/CLionProjects/llumc/cmake-build-debug/map.txt";
        outputMap(fileMap, dashMap);
    }
    return 0;
}

void outputMap(std::string fileMap, std::map<std::string, std::vector<unsigned int>> dashMap){
    std::cout << "Map is saved into file: " << fileMap << "\n";
    std::ofstream outfile;
    outfile.open(fileMap);
    for (auto & entry: dashMap) {
        //write s:0,1,2,3
        outfile << entry.first << ":";
        for (std::vector<int>::size_type i = 0; i != entry.second.size(); i++) {
            if (i == entry.second.size() - 1) {
                outfile << entry.second[i];
            } else {
                outfile << entry.second[i] << ",";
            }
        }
        outfile << "\n";
    }
    outfile.close();
}

void renameVariables(std::map<std::string, std::vector<unsigned int>> mapDash) {
    std::cout << "Highest Number: " << highestVar << "\n";
    std::ofstream out("DimSpecFormula.cnf", std::ofstream::app);
    std::ifstream in("DimSpecFormulaPre.cnf");
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            if (line.find("cnf") != std::string::npos) {
                int pos = line.find("f");
                std::stringstream start(line.substr(0, pos + 1));
                out << start.str() << " ";
                std::stringstream f(line.substr(pos + 1, line.size()));
                std::string s;
                int num = 0;
                if (line.find("t") != std::string::npos) {
                    int count = 0;
                    while (getline(f, s, ' ')) {
                        if (!s.empty()) {
                            if (count == 0) {
                                num = highestVar * 2;
                                count++;
                            } else {
                                num = atoi(s.c_str()) - 1;
                            }
                            out << num << " ";
                        }
                    }
                    out << "\n";
                } else {
                    int count = 0;
                    while (getline(f, s, ' ')) {
                        if (!s.empty()) {
                            if (count == 0) {
                                num = highestVar;
                                count++;
                            } else {
                                num = atoi(s.c_str()) - 1;
                            }
                            out << num << " ";
                        }
                    }
                    out << "\n";
                }
            } else {
                std::stringstream f(line);
                std::string s;

                while (getline(f, s, ' ')) {
                    //std::cout << s << ",";
                    bool found = false;
                    if (!s.empty()) {
                        int num = 0;
                        for (auto &entry: mapDash) {
                            int pos = 0;
                            for (auto sec : entry.second) {
                                if (entry.first.size() > 4 &&
                                    entry.first.substr(entry.first.size() - 4, entry.first.size()) == "Dash") {
                                    if (atoi(s.c_str()) == sec) {
                                        //std::cout << "Test, object was found. " << pos << " " << entry.first << " "
                                                 // << atoi(s.c_str()) << "\n";
                                        num = mapDash[entry.first.substr(0, entry.first.size() - 4)].at(pos) +
                                              highestVar;
                                        //std::cout << "rename: " << s << " -> " << num << "\n";
                                        found = true;
                                    }else if(-atoi(s.c_str())==sec){
                                        //std::cout << "Test, object was found. " << pos << " " << entry.first << " "
                                                // << atoi(s.c_str()) << "\n";
                                        num = -(mapDash[entry.first.substr(0, entry.first.size() - 4)].at(pos) +
                                              highestVar);
                                        //std::cout << "rename: " << s << " -> " << num << "\n";
                                        found = true;
                                    }
                                    pos++;
                                }
                            }
                        }
                        if (!found) {
                            num = atoi(s.c_str());
                        }
                        out << num << " ";
                    }
                }
                out << "\n";
            }
        }
    }
    in.close();
    out.close();
}

/**
 * summarizes the 4 output files into one
 */
void summarizeOutputFiles() {
    readFile("output_1.cnf", "i cnf");//init
    readFile("output_3.cnf", "u cnf");//univ
    readFile("output_2.cnf", "g cnf");//goal
    readFile("output_0.cnf", "t cnf");//trans
}

void readFile(std::string file, std::string type) {
    std::ofstream output("DimSpecFormulaPre.cnf", std::ofstream::app);
    std::ifstream init(file);
    if (init.fail()) {
        std::cout << "File does not exist\n";
        output << "\n" << type << " 1 1\n";
    } else {
        std::string line;
        int counter = 0;
        while (std::getline(init, line)) {
            if (counter == 0) {
                counter++;
                continue;
            }
            if (counter == 1) {
                output << type;
                int pos = line.find("f");
                std::stringstream f(line.substr(pos + 1, line.size()));
                std::string s;
                int co = 0;
                while (getline(f, s, ' ')) {
                    /* if (co == 0 && i == 1 && !s.empty()) {
                         highestVar = atoi(s.c_str());
                         std::cout << "high:" << highestVar << ", " << s << "\n";
                         co++;
                     }*/
                    output << s << " ";
                }
                output << "\n";
                counter++;
            } else {
                std::stringstream f(line);
                std::string s;
                while (getline(f, s, ' ')) {
                    output << s << " ";
                    if (highestVar < atoi(s.c_str())) {
                        highestVar = atoi(s.c_str());
                    }
                }
                output << "\n";
                //output << line << "\n";
            }
        }
    }
    init.close();
    output.close();
}

void removeOldFiles() {
    llvm::outs() << "Following files have been deleted: ";
    if (remove("AIGtoCNF.txt") == 0) {
        llvm::outs() << "AIGtoCNF.txt, ";
    }
    if (remove("NameToAIG.txt") == 0) {
        llvm::outs() << "NameToAIG.txt, ";
    }
    if (remove("output_0.cnf") == 0) {
        llvm::outs() << "output_0.cnf, ";
    }
    if (remove("output_1.cnf") == 0) {
        llvm::outs() << "output_1.cnf, ";
    }
    if (remove("output_2.cnf") == 0) {
        llvm::outs() << "output_2.cnf, ";
    }
    if (remove("output_3.cnf") == 0) {
        llvm::outs() << "output_3.cnf, ";
    }
    if (remove("DimSpecFormula.cnf") == 0) {
        llvm::outs() << "DimSpecFormula.cnf, ";
    }
    if (remove("DimSpecFormulaPre.cnf") == 0) {
        llvm::outs() << "DimSpecFormulaPre.cnf";
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


