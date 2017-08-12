//
// Created by marko on 10.07.17.
//

#ifndef LLUMC_IOBSERVER_H
#define LLUMC_IOBSERVER_H

#include <llbmc/Solver/DefaultSMTTranslator.h>

class IObserver
{
public:
    virtual void Update(SMT::BoolExp *expr) = 0;
};



#endif //LLUMC_IOBSERVER_H
