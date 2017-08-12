//
// Created by marko on 26.06.17.
//

#ifndef TRANSFORMERSOLVER_SOLVER_H
#define TRANSFORMERSOLVER_SOLVER_H

#include "SMTTranslator.h"
#include <iostream>
#include <llbmc/SMT/SMTLIB.h>
#include <llbmc/SMT/Solver.h>
#include <llbmc/Solver/SMTContext.h>
#include <llbmc/Output/Common.h>
#include <llbmc/SMT/STP.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/FileSystem.h>



class Solver {
public:
    enum SMTSolver
    {
        SMTLIB,
        STP,
        Boolector
    };

    void runSMTSolver();

    llvm::raw_ostream *ostream2();

};


#endif //TRANSFORMERSOLVER_SOLVER_H
