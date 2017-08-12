//
// Created by marko on 10.07.17.
//

#ifndef LLUMC_ASUBJECT_H
#define LLUMC_ASUBJECT_H

#pragma once
#include <vector>
#include <list>
#include "Encoder.h"

class ASubject
{
    //Lets keep a track of all the shops we have observing
    std::vector<Encoder*> list;

public:
    void Attach(Encoder *product);
    void Detach(Encoder *product);
    void Notify(SMT::BoolExp *expr);
};


#endif //LLUMC_ASUBJECT_H
