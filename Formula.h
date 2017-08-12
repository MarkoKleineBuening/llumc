//
// Created by marko on 10.07.17.
//

#ifndef LLUMC_FORMULA_H
#define LLUMC_FORMULA_H


#include "ASubject.h"

class Formula : public ASubject{

    void createFormula(SMT::BoolExp *expr);
};


#endif //LLUMC_FORMULA_H
