//
// Created by marko on 10.07.17.
//


#include "ASubject.h"
#include <algorithm>

using namespace std;

void ASubject::Attach(Encoder *enc)
{
    list.push_back(enc);
}
void ASubject::Detach(Encoder *enc)
{
    list.erase(std::remove(list.begin(), list.end(), enc), list.end());
}

void ASubject::Notify(SMT::BoolExp *expr)
{
    for(vector<Encoder*>::const_iterator iter = list.begin(); iter != list.end(); ++iter)
    {
        if(*iter != 0)
        {
           (*iter)->Update(expr);
        }
    }
}