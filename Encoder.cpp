//
// Created by marko on 03.07.17.
//


#include "Encoder.h"


Encoder::Encoder(llvm::Module &module, llbmc::SMTContext *context)
        : m_module(module), m_bbmap(), m_instructionEncoder(context),
          m_variableSet(), m_currentFormula(), m_SMTTranslator(*context),
          m_smtContext(context), m_variableMap() {
}

SMT::BoolExp *Encoder::encodeFormula(llvm::StringRef string) {
    llvm::Function *Func = m_module.getFunction(string);
    int numBB = Func->size();
    int width = ceil(log(numBB + 2) / log(2.0));
    std::cout << "Encoding of BasicBlocks: " << numBB << "\n";
    int counter = 1;
    for (llvm::BasicBlock &BB : *Func) {
        m_bbmap[BB.getName()] = counter++;
    }
    m_bbmap["ok"] = counter++;
    m_bbmap["error"] = counter;
    m_instructionEncoder.setBBMap(m_bbmap);
    SMT::BoolExp *boolExp = m_smtContext->getSatCore()->mk_true();
    for (llvm::BasicBlock &BB : *Func) {
        SMT::BoolExp *exp = encodeBasicBlockOverTerminator(BB, width);
        boolExp = m_smtContext->getSatCore()->mk_and(boolExp, exp);
        if (BB.getName().equals("bb3")) {
            //break;
        }
    }
    llvm::outs() << " ... finished\n";
    return boolExp;
}

SMT::BoolExp *Encoder::encodeBasicBlockOverTerminator(llvm::BasicBlock &bb, int width) {
    m_instructionEncoder.resetFormulaSet();
    m_instructionEncoder.resetNotUsedVar();
    llvm::outs() << "Formula Size At Start:" << m_instructionEncoder.getFormulaSet().size() << "\n";
    SMT::SatCore *satCore = m_smtContext->getSatCore();
    llvm::StringRef name = bb.getName();;
    llvm::outs() << "BB:" << name << "\n";
    //if s does not exist create else take out of map
    SMT::BVExp *bve = m_SMTTranslator.createBV("s", width);  // s
    SMT::BVExp *bveDash = m_SMTTranslator.createBV("sDash", width);  // sDash
    SMT::BVExp *bvePred = m_SMTTranslator.createBV("pred", width);  // pred
    SMT::BVExp *bvePredDash = m_SMTTranslator.createBV("predDash", width);  // pred
    SMT::BoolExp *boeFirst = m_SMTTranslator.bvAssignValue(bve, m_bbmap.at(name));  // s = entry
    SMT::BoolExp *boeFirstPred = m_SMTTranslator.bvAssignValue(bvePred, m_bbmap.at(name));  // s = entry
    boeFirst = satCore->mk_and(boeFirst, boeFirstPred);
    SMT::BoolExp *boePredDash = m_SMTTranslator.bvAssignValue(bvePredDash, m_bbmap.at(name)); //pred = entry

    SMT::BoolExp *boeTmp = boeFirst;
    llvm::TerminatorInst *termInst = bb.getTerminator();

    if (termInst->getNumOperands() == 3) { //br %1 b1, b2
        llvm::Instruction *inst = (llvm::Instruction *) termInst->getOperand(0);
        llvm::outs() << *inst << "\n";
        handleInstruction(*inst);

        //TODO llvm reorders the operands
        if (llvm::BranchInst *B = llvm::dyn_cast<llvm::BranchInst>(termInst)){
            llvm::outs() << " Operands have been switched. ";
            //TODO if(has swaped earlier)
            B->swapSuccessors();
        }
        //TODO END


        llvm::outs() << "First Operand:" << termInst->getOperand(1)->getName() << " / ";
        llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaSet = m_instructionEncoder.getFormulaSet();
        for (SMT::BoolExp *formula : formulaSet) {
            llvm::outs() << "Formula Size First Operand:" << formulaSet.size() << "\n";
            boeFirst = satCore->mk_and(boeFirst, formula); // s=entry && 4096 < x0
        }
        SMT::BoolExp *boeDashFirst = m_SMTTranslator.bvAssignValue(bveDash, m_bbmap.at(
                termInst->getOperand(1)->getName()));//sDash=bb1
        boeDashFirst = satCore->mk_and(boeDashFirst, boePredDash);
        boeDashFirst = satCore->mk_and(boeDashFirst, same());


        llvm::outs() << "Second Operand:" << termInst->getOperand(2)->getName() << " / ";
        llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaSetICMP = m_instructionEncoder.getFormulaSetICMP();
        SMT::BoolExp *boeSecond = boeTmp;
        for (SMT::BoolExp *formula : formulaSetICMP) {
            llvm::outs() << "Formula Size ICMP at Second Operand:" << formulaSet.size() << "\n";
            boeSecond = satCore->mk_and(boeSecond, satCore->mk_not(formula)); // s=entry && not(4096 < x0)
        }

        SMT::BoolExp *boeDash = m_SMTTranslator.bvAssignValue(bveDash, m_bbmap.at(termInst->getOperand(2)->getName()));//sDash=bb2
        boeDash = satCore->mk_and(boeDash, boePredDash);
        boeDash = satCore->mk_and(boeDash, same());

        SMT::BoolExp *saveVarAssume = satCore->mk_true();
        //check instructions before branch jump
        SMT::BoolExp *saveVar;

        //reverse list.
        for (llvm::BasicBlock::reverse_iterator I = bb.rbegin(); I != bb.rend(); ++I) {
            saveVar = satCore->mk_true();
            if (I->isTerminator()) { continue; }
            //llvm::outs() << "Iterate over Instructions:"<<I->getName()<<"\n";
            const bool is_in = m_variableMap.find(I->getName()) != m_variableMap.end();
            if (is_in) {
                handleInstruction(*I);
                llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaSetSave = m_instructionEncoder.getFormulaSetSave();
                llvm::outs() << "Formula Size Save:" << formulaSetSave.size() << "\n";
                for (SMT::BoolExp *formula : formulaSetSave) {
                    saveVar = satCore->mk_and(saveVar, formula);
                }
                boeDashFirst = satCore->mk_and(boeDashFirst, saveVar);
                boeDash = satCore->mk_and(boeDash, saveVar);
                m_instructionEncoder.resetFormulaSetSave();
            }

            if (llvm::CallInst *C = llvm::dyn_cast<llvm::CallInst>(&I.operator*())) { //handle assume
                if (llbmc::FunctionMatcher::isSpecification(*C)) {
                    handleInstruction(*I);
                    llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaSetSave = m_instructionEncoder.getFormulaSetSave();
                    llvm::outs() << "Formula Size Assume/Call:" << formulaSetSave.size() << "\n";
                    if (formulaSetSave.size() > 0) {
                        saveVarAssume = satCore->mk_and(saveVarAssume, *formulaSetSave.begin());
                        saveVarAssume = satCore->mk_and(saveVarAssume, same());
                    }
                }
            }
        }

        SMT::BoolExp *firstOperand = satCore->mk_implies(boeFirst, boeDashFirst);
        SMT::BoolExp *secondOperand = satCore->mk_implies(boeSecond, boeDash);
        secondOperand = satCore->mk_and(secondOperand, saveVarAssume);

        SMT::BoolExp *formula = satCore->mk_and(firstOperand, secondOperand);
        llvm::outs() << "\n-------------------------------\n";
        return formula;
    } else if (termInst->getNumOperands() == 1) { //br b1 or return i32 0 //TODO better: B->isUnconditional()
        if (termInst->mayReturn() && bb.getName().startswith("return")) {
            llvm::outs() << "RETURN STATEMENT:\n";
            //s=current and 0 --> sDash = ok
            //s=current and not(0) --> sDash = error
            m_instructionEncoder.visitReturn(*termInst);
            SMT::BoolExp *save = nullptr;
            if (m_instructionEncoder.getFormulaSetSave().size() > 0) {
                save = *m_instructionEncoder.getFormulaSetSave().begin();
            }
            save = satCore->mk_and(save, same());
            return save;
        } else {
            llvm::outs() << "Only Operand:" << termInst->getOperand(0)->getName() << " / ";
            llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaSet = m_instructionEncoder.getFormulaSet();
            SMT::BVExp *bveDash = m_SMTTranslator.createBV("sDash", width);  // s
            SMT::BoolExp *boeDash = m_SMTTranslator.bvAssignValue(bveDash, m_bbmap.at(
                    termInst->getOperand(0)->getName()));//sDash=bb1
            boeDash = satCore->mk_and(boeDash, boePredDash);

            //check instructions before branch jump
            SMT::BoolExp *saveVarAssume = satCore->mk_true();
            SMT::BoolExp *saveVar = satCore->mk_true();
            for (llvm::BasicBlock::reverse_iterator I = bb.rbegin(); I != bb.rend(); ++I) {
                llvm::outs() << "Iterate Reverse:" << I->getName() << "\n";
                if (I->isTerminator()) { continue; }
                const bool is_in = m_variableMap.find(I->getName()) != m_variableMap.end();
                if (is_in) {
                    handleInstruction(*I);
                    llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaSetSave = m_instructionEncoder.getFormulaSetSave();
                    llvm::outs() << "Formula Size Save Only Operand("<<I->getName()<<"):" << formulaSetSave.size() << "\n";
                    for (SMT::BoolExp *formula : formulaSetSave) {
                        saveVar = satCore->mk_and(saveVar, formula);
                    }
                    boeDash = satCore->mk_and(boeDash, saveVar);
                    m_instructionEncoder.resetFormulaSetSave();
                }

                if (llvm::CallInst *C = llvm::dyn_cast<llvm::CallInst>(&I.operator*())) { //handle assume
                    if (llbmc::FunctionMatcher::isSpecification(*C)) {
                        // make sure it is assume and not other call
                        handleInstruction(*I);
                        llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaSetSave = m_instructionEncoder.getFormulaSetSave();
                        llvm::outs() << "Formula Size Assume/Call Only Operand:" << formulaSetSave.size() << "\n";
                        if (formulaSetSave.size() > 0) {
                            saveVarAssume = satCore->mk_and(saveVarAssume, *formulaSetSave.begin());
                            saveVarAssume = satCore->mk_and(saveVarAssume, same());
                        }
                    }
                }
            }
            boeDash = satCore->mk_and(boeDash, same());
            SMT::BoolExp *onlyOperand = satCore->mk_implies(boeFirst, boeDash);
            SMT::BoolExp *onlyOperandWithAssume = satCore->mk_and(onlyOperand, saveVarAssume);
            llvm::outs() << "\n-------------------------------\n";
            return onlyOperandWithAssume;
        }
    } else { //unreachable
        llvm::outs() << "Unreachable Terminator has to be regarded!\n";
        SMT::BoolExp *aboveUnreachable = satCore->mk_true();
        SMT::BoolExp *saveVar;
        for (llvm::BasicBlock::reverse_iterator I = bb.rbegin(); I != bb.rend(); ++I) {
            if (I->isTerminator()) { continue; }
            handleInstruction(*I);
            llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaSetSave = m_instructionEncoder.getFormulaSetSave();
            llvm::outs() << "Formula Size Above Unrechable:" << formulaSetSave.size() << "\n";
            if (formulaSetSave.size() > 0) {
                aboveUnreachable = satCore->mk_and(aboveUnreachable, *formulaSetSave.begin());
            }
        }
        aboveUnreachable = satCore->mk_and(aboveUnreachable, same());
        llvm::outs() << "\n-------------------------------\n";
        return aboveUnreachable;
    }
    return nullptr;
}

/*
 * returns a formula that states that all other variables stay unchanged
 */
SMT::BoolExp *Encoder::same() {
    //s,sDash,pred,predDash and all variables that are contained in firstOperand
    SMT::SatCore *satCore = m_smtContext->getSatCore();
    SMT::BoolExp *boolExp = satCore->mk_true();
    int width = -1;
    std::set<llvm::StringRef> notUsedVar = m_instructionEncoder.getNotUsedVar();
    for (llvm::StringRef str : notUsedVar) {
        width = m_variableMap.at(str);
        SMT::BVExp *bv = m_SMTTranslator.createBV(str, width);
        std::string string = str;
        std::string strDash = string + "Dash";
        SMT::BVExp *bvDash = m_SMTTranslator.createBV(strDash, width);
        SMT::BoolExp *eq = m_SMTTranslator.compare(bv, bvDash, llvm::CmpInst::ICMP_EQ);
        boolExp = satCore->mk_and(boolExp, eq);
    }
    return boolExp;
}

void Encoder::calculateState(llvm::StringRef string) {
    bool isUsed = false;
    llvm::Function *func = m_module.getFunction(string);
    int numBB = func->size();
    for (llvm::BasicBlock &BB : *func) {
        for (llvm::Instruction &I : BB) {
            isUsed = false;
            //llvm::outs() << I << ":"<< BB.getName() << "\n";
            for (auto &use : I.uses()) {

                //llvm::outs() <<"-----" << *(use.getUser())<< "  /";
                //handles BranchInst a litte bit different because you have to look at the firstOperand
                if (llvm::BranchInst *B = llvm::dyn_cast<llvm::BranchInst>(use.getUser())) {
                    isUsed = isOnlyUsedInBasicBlock(string, &BB, B->getOperand(0));
                } else if (llvm::CallInst *C = llvm::dyn_cast<llvm::CallInst>(use.getUser())) {
                    //TODO handle llbm_assume, edit FunctionMatcher to recognize assume and then use Operand not output
                    isUsed = isOnlyUsedInBasicBlock(string, &BB, use.getUser());
                } else {
                    isUsed = isOnlyUsedInBasicBlock(string, &BB, use.getUser());
                }
                //llvm::outs() << "used in BB:" <<isUsed<< "\n";

                if (!isUsed) {
                    //add variable to state
                    //llvm::outs() << "   USED!";
                    //llvm::outs() << I.getValueName()->getKey() << "\n";
                    //numBB +2 because there is one "ok" node and one "error" node
                    int width = ceil(log(numBB + 2) / log(2.0));
                    for (int i = 0; i < I.getNumOperands(); i++) {
                        width = I.getType()->getIntegerBitWidth();
                    }
                    m_variableSet.insert(I.getValueName()->getKey());
                    m_variableMap[I.getValueName()->getKey()] = width;
                    llvm::outs() << I.getValueName()->getKey() << "," << width << "/ ";
                    break;
                }
            }
        }
    }
    m_instructionEncoder.setVariableList(m_variableSet);
    llvm::outs() << "... finished\n";
}

bool Encoder::isOnlyUsedInBasicBlock(llvm::StringRef string, llvm::BasicBlock *currentBB, llvm::Value *user) {
    bool isUsed;
    llvm::Function *func = m_module.getFunction(string);
    for (llvm::BasicBlock &BB : *func) {
        if (BB.getName() != currentBB->getName()) {
            //TODO isUsedInBasicBlock is buggy, Example: finds x.0 and tmp15 in BasicBlock:bb5
            //isUsed = user->isUsedInBasicBlock(&BB);
            isUsed = isUsedInBasicBlock(string, &BB, user);
            //llvm::outs() << ":::" << currentBB->getName()<< ", " <<BB.getName()<<", " << isUsed << "\n";
            if (isUsed) {
                return false;
            }
        }
    }
    return true;
}

bool Encoder::isUsedInBasicBlock(llvm::StringRef string, llvm::BasicBlock *BB, llvm::Value *user) {
    bool isUsed;
    llvm::Function *func = m_module.getFunction(string);
    llvm::Instruction *User = llvm::dyn_cast<llvm::Instruction>(user);
    if (User->getParent() == BB) {
        //llvm::outs() <<"\nIsUsedInBasicBlock:" << User << ","<< BB->getName() << "\n";
        return true;
    }
    return false;
}

void Encoder::handleInstruction(llvm::Instruction &I) {
    if (llvm::CallInst *C = llvm::dyn_cast<llvm::CallInst>(&I)) {
        handleCallNode(C);
    } else if (llvm::PHINode *P = llvm::dyn_cast<llvm::PHINode>(&I)) {
        std::cout << "PHINode" << std::endl;
        m_instructionEncoder.visit(I);//TODO need distinction?
    } else if (llvm::ReturnInst *R = llvm::dyn_cast<llvm::ReturnInst>(&I)) {
        std::cout << "ReturnNode" << std::endl;
    } else {
        m_instructionEncoder.visit(I);
    }
}

void Encoder::handleCallNode(llvm::CallInst *C) {
    assert(C->getValueID() == 68);
    if (llbmc::FunctionMatcher::isCalloc(*C)) {
        std::cout << "isCalloc" << std::endl;
    } else if (llbmc::FunctionMatcher::isFree(*C)) {
        std::cout << "isFree" << std::endl;
    } else if (llbmc::FunctionMatcher::isSpecification(*C)) {
        m_instructionEncoder.visit(C);
    } else if (llbmc::FunctionMatcher::isAssert(*C)) {
        std::cout << "isAssert" << std::endl;
        m_instructionEncoder.visit(C);
    } else if (llbmc::FunctionMatcher::isAssertFail(*C)) {
        std::cout << "isAssertFail" << std::endl;
        m_instructionEncoder.visit(C);
    } else {
        std::cout << "Undefined Call" << std::endl;
    }
}

void Encoder::Update(SMT::BoolExp *expr) {
    m_currentFormula = expr;
}

SMT::BoolExp *Encoder::getInitialExp() {
    //return s = 0;
    int numBB = m_bbmap.size();
    int width = ceil(log(numBB) / log(2.0));
    SMT::BVExp *sBv = m_SMTTranslator.createBV("s", width);
    SMT::BoolExp *ret = m_SMTTranslator.bvAssignValue(sBv, 0);
    return ret;
    //TODO add pred = 0 !!!
}

SMT::BoolExp *Encoder::getGoalExp() {
    //return s = ok;
    int numBB = m_bbmap.size();
    int width = ceil(log(numBB) / log(2.0));
    SMT::BVExp *sBv = m_SMTTranslator.createBV("s", width);
    SMT::BoolExp *ret = m_SMTTranslator.bvAssignValue(sBv, m_bbmap.at("ok"));
    return ret;
}

SMT::BoolExp *Encoder::getUniversalExp() {
    int numBB = m_bbmap.size();
    int width = ceil(log(numBB) / log(2.0));
    SMT::BVExp *sBv = m_SMTTranslator.createBV("s", width);
    SMT::BVExp *th = m_SMTTranslator.createConst(numBB-1,width);
    SMT::BoolExp *retS = m_SMTTranslator.compare(sBv, th, llvm::CmpInst::ICMP_SLE);
    sBv = m_SMTTranslator.createBV("pred", width);
    th = m_SMTTranslator.createConst(numBB-1,width);
    SMT::BoolExp *retPred = m_SMTTranslator.compare(sBv, th, llvm::CmpInst::ICMP_SLE);
    SMT::BoolExp *ret = m_smtContext->getSatCore()->mk_and(retS, retPred);
    return ret;
}

SMT::BoolExp *Encoder::getUniversalExpReverse() {
    int numBB = m_bbmap.size();
    int width = ceil(log(numBB) / log(2.0));

    SMT::BVExp *sBv = m_SMTTranslator.createBV("pred", width);
    SMT::BVExp *th = m_SMTTranslator.createConst(numBB-1,width);
    SMT::BoolExp *retPred = m_SMTTranslator.compare(sBv, th, llvm::CmpInst::ICMP_SLE);
    sBv = m_SMTTranslator.createBV("s", width);
    th = m_SMTTranslator.createConst(numBB-1,width);
    SMT::BoolExp *retS = m_SMTTranslator.compare(sBv, th, llvm::CmpInst::ICMP_SLE);
    SMT::BoolExp *ret = m_smtContext->getSatCore()->mk_and(retPred,retS);
    return ret;
}

SMT::BoolExp *Encoder::getBreaker() {
    return m_SMTTranslator.bvAssignValue(m_SMTTranslator.createBV("breaker",1), 1);
}

SMT::BoolExp *Encoder::getBreaker1() {
    return m_SMTTranslator.bvAssignValue(m_SMTTranslator.createBV("breaker1",1), 1);
}

SMT::BoolExp *Encoder::getBreaker2() {
    return m_SMTTranslator.bvAssignValue(m_SMTTranslator.createBV("breaker2",1), 1);
}












