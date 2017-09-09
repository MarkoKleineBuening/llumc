//
// Created by marko on 23.06.17.
//

#ifndef TRANSFORMERSOLVER_SMTTRANSLATOR_H
#define TRANSFORMERSOLVER_SMTTRANSLATOR_H


#include <llbmc/Solver/DefaultSMTTranslator.h>
#include <llvm/IR/InstrTypes.h>


class SMTTranslator  : public llbmc::SMTTranslatorBase {

public:

    SMTTranslator(llbmc::SMTContext &context);

    SMT::BoolExp * visitSimpleBranch();

    SMT::BoolExp *archiv();

    SMT::BoolExp *visitSimpleBranchCorrect();

    SMT::BVExp *createBV(std::string name, int width);

    SMT::BoolExp *bvAssignValue(SMT::BVExp *bitvector, int value);

    SMT::BoolExp *compareSlt(SMT::BVExp *a, SMT::BVExp *b);

    SMT::BoolExp *bvImplies(SMT::BVExp *an, SMT::BVExp *cn, SMT::SatCore *pCore);

    SMT::BVExp *createConst(int value, int width);

    SMT::BoolExp *bvAssignNegationValue(SMT::BVExp *pExp, int i);

    SMT::BoolExp *bvAssignValueNeg(SMT::BVExp *pExp, int i);

    SMT::BoolExp *compare(SMT::BVExp *pExp, SMT::BVExp *pBVExp, llvm::CmpInst::Predicate predicate);

    SMT::BVExp *add(SMT::BVExp *bv1, SMT::BVExp *bv2);

    SMT::BVExp *zext(SMT::BVExp *exp, unsigned int i);

    SMT::BVExp *cond(SMT::BoolExp *boe, SMT::BVExp *bv1, SMT::BVExp *bv2);

    SMT::BVExp *doOr(SMT::BVExp *pExp, SMT::BVExp *pBVExp);

    SMT::BVExp *doAnd(SMT::BVExp *pExp, SMT::BVExp *pBVExp);

    SMT::BVExp *mul(SMT::BVExp *pExp, SMT::BVExp *pBVExp);

    SMT::BVExp *ashr(SMT::BVExp *pExp, SMT::BVExp *pBVExp);

    SMT::BVExp *lshr(SMT::BVExp *pExp, SMT::BVExp *pBVExp);

    SMT::BVExp *shl(SMT::BVExp *pExp, SMT::BVExp *pBVExp);

    SMT::BVExp *srem(SMT::BVExp *pExp, SMT::BVExp *pBVExp);

    SMT::BVExp *urem(SMT::BVExp *pExp, SMT::BVExp *pBVExp);

    SMT::BVExp *sub(SMT::BVExp *pExp, SMT::BVExp *pBVExp);

private:
    std::map<std::string, SMT::BVExp*> m_bvSet;


};


#endif //TRANSFORMERSOLVER_SMTTRANSLATOR_H
