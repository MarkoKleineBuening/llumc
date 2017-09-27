//
// Created by marko on 23.06.17.
//

#include <iostream>
#include <llvm/Support/raw_ostream.h>
#include "SMTTranslator.h"

SMTTranslator::SMTTranslator(llbmc::SMTContext &context) : SMTTranslatorBase(context), m_bvSet() {

}

SMT::BVExp *SMTTranslator::createBV(std::string name, int width){
    const bool is_in = m_bvSet.find(name) != m_bvSet.end();
    if(is_in) {
        return m_bvSet.at(name);
    }else{
        SMT::BVExp *s = bv().bitvector(width, name);
        m_bvSet[name] = s;
        return s;
    }
}

SMT::BVExp *SMTTranslator::createConst(int value, int width) {
    SMT::BVExp *constant = bv().bv2bv(new Bitvector(value, width));
    return constant;
}




SMT::BoolExp *SMTTranslator::bvAssignValue(SMT::BVExp *bitvector, int value) {
    SMT::BVExp *s = bitvector;
    SMT::BVExp *constant = bv().bv2bv(new Bitvector(value, bv().width(bitvector)));
    SMT::BoolExp *an = bv().eq(s, constant);
    return an;
}

SMT::BoolExp *SMTTranslator::bvAssignValueNeg(SMT::BVExp *bitvector, int value) {
    SMT::BVExp *s = bitvector;
    SMT::BVExp *constant = bv().bv2bv(new Bitvector(value, bv().width(bitvector)));
    SMT::BoolExp *an = bv().bvne(s, constant);
    return an;
}

SMT::BoolExp *SMTTranslator::bvAssignNegationValue(SMT::BVExp *bitvector, int value) {
    SMT::BVExp *s = bitvector;
    SMT::BVExp *constant = bv().bv2bv(new Bitvector(value, bv().width(bitvector)));
    SMT::BVExp *negation = bv().bvneg(constant);
    SMT::BoolExp *an = bv().eq(s, negation);
    return an;
}

SMT::BoolExp *SMTTranslator::compareSlt(SMT::BVExp *a, SMT::BVExp *b){
    SMT::BoolExp *ret = bv().bvslt(a,b);
    return ret;
}

SMT::BVExp *SMTTranslator::add(SMT::BVExp *bv1, SMT::BVExp *bv2) {
    SMT::BVExp *ret = bv().bvadd(bv1,bv2);
    return ret;
}

SMT::BVExp *SMTTranslator::cond(SMT::BoolExp *boe, SMT::BVExp *bv1, SMT::BVExp *bv2){
    return bv().bvcond(boe, bv1, bv2);
}

SMT::BoolExp *SMTTranslator::compare(SMT::BVExp *bv1, SMT::BVExp *bv2, llvm::CmpInst::Predicate predicate) {
    SMT::BoolExp *ret;
    switch (predicate) {
        case llvm::CmpInst::ICMP_EQ:
            ret = bv().eq(bv1,bv2);
            break;
        case llvm::CmpInst::ICMP_NE:
            ret = bv().bvne(bv1,bv2);
            break;
        case llvm::CmpInst::ICMP_UGT:
            ret = bv().bvugt(bv1,bv2);
            break;
        case llvm::CmpInst::ICMP_UGE:
            ret = bv().bvuge(bv1,bv2);
            break;
        case llvm::CmpInst::ICMP_ULT:
            ret = bv().bvult(bv1,bv2);
            break;
        case llvm::CmpInst::ICMP_ULE:
            ret = bv().bvule(bv1,bv2);
            break;
        case llvm::CmpInst::ICMP_SGT:
            ret = bv().bvsgt(bv1,bv2);
            break;
        case llvm::CmpInst::ICMP_SGE:
            ret = bv().bvsge(bv1,bv2);
            break;
        case llvm::CmpInst::ICMP_SLT:
            ret = bv().bvslt(bv1,bv2);
            break;
        case llvm::CmpInst::ICMP_SLE:
            ret = bv().bvsle(bv1,bv2);
            break;
        default:
            llvm::outs() << "ERROR!";
    }
    return ret;
}

//-------------------------------//

SMT::BoolExp *SMTTranslator::bvImplies(SMT::BVExp *an, SMT::BVExp *cn, SMT::SatCore *pCore) {

    SMT::BVExp * neg = bv().bvnot(an);
    SMT::BVExp *orAC = bv().bvor(neg, cn);
    SMT::BoolExp *bo = bv().bv12bool(orAC);
    return  bo;
}

SMT::BoolExp *SMTTranslator::archiv() {
    SMT::BVExp *s = bv().bitvector(4, "s");
    SMT::BVExp *sDash = bv().bitvector(4, "sDash");
    SMT::BVExp *const3 = bv().bv2bv(new Bitvector(3, 4));
    SMT::BVExp *const4 = bv().bv2bv(new Bitvector(4, 4));

    SMT::BoolExp *an = bv().eq(s, const3);
    SMT::BoolExp *cn = bv().eq(sDash, const4);

    SMT::BVExp *anBV = bv().bool2bv1(an);
    SMT::BVExp *cnBV = bv().bool2bv1(cn);
    SMT::BVExp *branch = bv().bvimplies(anBV, cnBV);

    SMT::BVExp *a = bv().bvand(cnBV, bv().bvneg(branch));
    SMT::BVExp *b = bv().bvand(branch, bv().bvneg(cnBV));
    SMT::BVExp *c = bv().bvand(a, b);
    SMT::BVExp *d = bv().bvand(c, bv().bvneg(c));
    SMT::BVExp *e = bv().bvand(d, bv().bvnot(c));
    SMT::BVExp *branchNot = bv().bvneg(branch);


    SMT::BoolExp *f = bv().bv12bool(e);

    SMT::BVExp *const5 = bv().bv2bv(new Bitvector(5, 4));
    SMT::BVExp *const6 = bv().bv2bv(new Bitvector(6, 4));
    SMT::BoolExp *eqUnsat = bv().eq(const5, const6);

    return eqUnsat;
}

/*
 * returns: s=3 --> s'= 4
 */
SMT::BoolExp *SMTTranslator::visitSimpleBranch() {
    SMT::BVExp *s = bv().bitvector(4, "s");
    SMT::BVExp *sDash = bv().bitvector(4, "sDash");
    SMT::BVExp *const3 = bv().bv2bv(new Bitvector(3, 4));
    SMT::BVExp *const4 = bv().bv2bv(new Bitvector(4, 4));

    SMT::BoolExp *an = bv().eq(s, const3);
    SMT::BoolExp *cn = bv().eq(sDash, const4);

    SMT::BVExp *anBV = bv().bool2bv1(an);
    SMT::BVExp *cnBV = bv().bool2bv1(cn);
    SMT::BVExp *anda = bv().bvmul(anBV, cnBV);
    SMT::BVExp *branch = bv().bvimplies(anBV, cnBV);

    SMT::BoolExp *outSat = bv().bv12bool(branch);
    return outSat;
}

SMT::BoolExp *SMTTranslator::visitSimpleBranchCorrect() {
    SMT::BVExp *s = bv().bitvector(4, "s");
    SMT::BVExp *sDash = bv().bitvector(4, "sDash");
    SMT::BVExp *const3 = bv().bv2bv(new Bitvector(3, 4));
    SMT::BVExp *const4 = bv().bv2bv(new Bitvector(4, 4));

    SMT::BoolExp *an = bv().eq(s, const3);
    SMT::BoolExp *cn = bv().eq(sDash, const4);

    SMT::BVExp *branch = bv().bvimplies(s, sDash);

    SMT::BoolExp *outSat = bv().bv12bool(branch);
    return outSat;
}

SMT::BVExp *SMTTranslator::doXor(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvxor(pExp, pBVExp);
}

SMT::BVExp *SMTTranslator::zext(SMT::BVExp *exp, unsigned int width) {
   return bv().uext(exp, width);
}

SMT::BVExp *SMTTranslator::sext(SMT::BVExp *exp, unsigned int width) {
    return bv().sext(exp, width);
}

SMT::BVExp *SMTTranslator::doOr(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvor(pExp, pBVExp);
}

SMT::BVExp *SMTTranslator::doAnd(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvand(pExp, pBVExp);
}

SMT::BVExp *SMTTranslator::mul(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvmul(pExp, pBVExp);
}

SMT::BVExp *SMTTranslator::ashr(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvashr(pExp,pBVExp);
}

SMT::BVExp *SMTTranslator::lshr(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvlshr(pExp,pBVExp);
}

SMT::BVExp *SMTTranslator::shl(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvshl(pExp,pBVExp);
}

SMT::BVExp *SMTTranslator::srem(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvsrem(pExp,pBVExp);
}

SMT::BVExp *SMTTranslator::urem(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvurem(pExp,pBVExp);
}

SMT::BVExp *SMTTranslator::sub(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvsub(pExp,pBVExp);
}

SMT::BoolExp *SMTTranslator::smulo(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvsmulo(pExp,pBVExp);
}

SMT::BoolExp *SMTTranslator::umulo(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvumulo(pExp,pBVExp);
}

SMT::BoolExp *SMTTranslator::saddo(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvsaddo(pExp, pBVExp);
}

SMT::BoolExp *SMTTranslator::uaddo(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvuaddo(pExp, pBVExp);
}

SMT::BoolExp *SMTTranslator::ssubo(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvssubo(pExp,pBVExp);
}

SMT::BoolExp *SMTTranslator::usubo(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvusubo(pExp,pBVExp);
}

SMT::BoolExp *SMTTranslator::sdivo(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvsdivo(pExp,pBVExp);
}

SMT::BVExp *SMTTranslator::udiv(SMT::BVExp *pExp, SMT::BVExp *pBVExp) {
    return bv().bvudiv(pExp,pBVExp);
}










