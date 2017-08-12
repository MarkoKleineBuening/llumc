//
// Created by marko on 26.06.17.
//

#include "Solver.h"


void Solver::runSMTSolver() {
    SMT::Solver::Result::Satisfiable;
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

    llbmc::SMTContext *context = new llbmc::SMTContext(solver, 4);
    SMTTranslator *translator = new SMTTranslator(*context);

    // assert Constraints einzeln s = 3, s'=4, s --> s'
    SMT::BVExp *s = translator->createBV("s", 4);
    SMT::BoolExp *sValue = translator->bvAssignValue(s, 3);


    SMT::BVExp *sDash = translator->createBV("sDash", 4);
    SMT::BoolExp *sDashValue = translator->bvAssignValue(sDash, 4);


    SMT::BoolExp *imp = satCore->mk_implies(sValue, sDashValue);
    solver->assertConstraint(imp);
    //solver->assertConstraint(sValue);
    //solver->assertConstraint(sDashValue);


        SMT::BVExp *number = translator->createConst(4096, 9);
        SMT::BVExp *x = translator->createBV("x", 9);
        SMT::BoolExp *slt = translator->compareSlt(number, x);
        solver->assertConstraint(slt);
/*
        SMT::BoolExp *sValueNeg = translator->bvAssignValueNeg(s, 3);
        SMT::BoolExp *notSValue = satCore->mk_not(sValue);
        SMT::BoolExp *orImp = satCore->mk_or(sValueNeg, sDashValue);
     */



    //SMT::BoolExp *test = satCore->mk_implies(sValue,satCore->mk_not(sValue) );
    //solver->assertConstraint(test);



    solver->solve();

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
}

llvm::raw_ostream *Solver::ostream2() {
    llvm::StringRef sRefName("/home/marko/test");
    std::error_code err_code;
    llvm::raw_fd_ostream *raw = new llvm::raw_fd_ostream(sRefName, err_code, llvm::sys::fs::OpenFlags::F_Append);
    return raw;
}
