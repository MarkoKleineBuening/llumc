//
// Created by marko on 03.07.17.
//


#include "Encoder.h"

//TODO: delete m_testing and all related code
Encoder::Encoder(llvm::Module &module, llbmc::SMTContext *context)
        : m_module(module), m_bbmap(), m_instructionEncoder(context),
          m_variableSet(), m_currentFormula(), m_SMTTranslator(*context),
          m_smtContext(context), m_variableMap(), m_testing(false), m_testing2(true), m_testing3(false) {
}

SMT::BoolExp *Encoder::encodeFormula(llvm::StringRef string) {
    llvm::Function *Func = m_module.getFunction(string);
    int numBB = Func->size();
    //std::cout << "Func size: " << numBB << "\n";
    int width = ceil(log(numBB + 2) / log(2.0));
    //std::cout << "WIdth: " << width << "\n";
    //std::cout << "Encoding of BasicBlocks: " << numBB << "\n";
    int counter = 1;
    for (llvm::BasicBlock &BB : *Func) {
        m_bbmap[BB.getName()] = counter++;
    }
    m_bbmap["ok"] = counter++;
    m_bbmap["error"] = counter;
    //std::cout << "m_bbmap.size: " << m_bbmap.size() << "\n";
    m_instructionEncoder.setBBMap(m_bbmap);
    SMT::BoolExp *boolExp = m_smtContext->getSatCore()->mk_true();
    for (llvm::BasicBlock &BB : *Func) {
        SMT::BoolExp *exp = encodeBasicBlockOverTerminator(BB, width);
        boolExp = m_smtContext->getSatCore()->mk_and(boolExp, exp);
        std::string entryName = "entry";
        /* if (BB.getName().equals(entryName)) {       break;     } */
    }
    SMT::BoolExp *error = encodeSingleBlock(width, "error");
    SMT::BoolExp *ok = encodeSingleBlock(width, "ok");
    boolExp = m_smtContext->getSatCore()->mk_and(boolExp, error);
    boolExp = m_smtContext->getSatCore()->mk_and(boolExp, ok);
    //llvm::outs() << " ... finished\n";
    return boolExp;
}

SMT::BoolExp *Encoder::encodeBasicBlockOverTerminator(llvm::BasicBlock &bb, int width) {
    m_instructionEncoder.resetFormulaSet();
    m_instructionEncoder.resetNotUsedVar();
    //llvm::outs() << "Formula Size At Start:" << m_instructionEncoder.getFormulaSet().size() << "\n";
    SMT::SatCore *satCore = m_smtContext->getSatCore();
    llvm::StringRef name = bb.getName();;
    llvm::outs() << "BB:" << name << ", " << width << "\n";
    //if s does not exist create else take out of map
    SMT::BVExp *bve = m_SMTTranslator.createBV("s", width);  // s
    SMT::BVExp *bveDash = m_SMTTranslator.createBV("sDash", width);  // sDash
    SMT::BVExp *bvePred = m_SMTTranslator.createBV("pred", width);  // pred
    SMT::BVExp *bvePredDash = m_SMTTranslator.createBV("predDash", width);  // pred
    SMT::BoolExp *boeFirst = m_SMTTranslator.bvAssignValue(bve, m_bbmap.at(name));  // s = entry
    std::string entryName = "entry";
    if (name == entryName) {
        //llvm::outs() << "Pred added for first Block\n";
        SMT::BoolExp *boeFirstPred = m_SMTTranslator.bvAssignValue(bvePred, m_bbmap.at(name));  // pred = entry
        boeFirst = satCore->mk_and(boeFirst, boeFirstPred);
    }

    SMT::BoolExp *boePredDash = m_SMTTranslator.bvAssignValue(bvePredDash, m_bbmap.at(name)); //pred = entry

    SMT::BoolExp *boeTmp = boeFirst;
    llvm::TerminatorInst *termInst = bb.getTerminator();
    //llvm::outs() << *termInst << "\n";
    if (termInst->getNumOperands() == 3) { //br %1 b1, b2
        llvm::Instruction *inst = (llvm::Instruction *) termInst->getOperand(0);
        //llvm::outs() << *inst << "\n";
        handleInstruction(*inst); //saves result in m_instructionEncoder.getFormulaSet() and others

        if (llvm::BranchInst *B = llvm::dyn_cast<llvm::BranchInst>(termInst)) {
            //llvm::outs() << " Operands have been switched. ";
            B->swapSuccessors();
        }

        //llvm::outs() << "First Operand:" << termInst->getOperand(1)->getName() << " / ";
        llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaSet = m_instructionEncoder.getFormulaSet();
        for (SMT::BoolExp *formula : formulaSet) {
            //llvm::outs() << "Formula Size First Operand:" << formulaSet.size() << "\n";
            boeFirst = satCore->mk_and(boeFirst, formula); // s=entry && 4096 < x0
        }
        SMT::BoolExp *boeDashFirst = m_SMTTranslator.bvAssignValue(bveDash, m_bbmap.at(
                termInst->getOperand(1)->getName()));//sDash=bb1
        boeDashFirst = satCore->mk_and(boeDashFirst, boePredDash);

        //llvm::outs() << "Second Operand:" << termInst->getOperand(2)->getName() << " / ";
        llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaSetICMP = m_instructionEncoder.getFormulaSetICMP();
        SMT::BoolExp *boeSecond = boeTmp;
        for (SMT::BoolExp *formula : formulaSetICMP) {
            //llvm::outs() << "Formula Size ICMP at Second Operand:" << formulaSet.size() << "\n";
            boeSecond = satCore->mk_and(boeSecond, satCore->mk_not(formula)); // s=entry && not(4096 < x0)
        }

        SMT::BoolExp *boeDash = m_SMTTranslator.bvAssignValue(bveDash, m_bbmap.at(
                termInst->getOperand(2)->getName()));//sDash=bb2
        boeDash = satCore->mk_and(boeDash, boePredDash);

        SMT::BoolExp *saveVarAssume = satCore->mk_true();
        //check instructions before branch jump
        SMT::BoolExp *saveVar;

        //reverse list.
        for (llvm::BasicBlock::reverse_iterator I = bb.rbegin(); I != bb.rend(); ++I) {
            saveVar = satCore->mk_true();
            if (I->isTerminator()) { continue; }
            //llvm::outs() << "Iterate over Instructions:" << I->getName() << "\n";
            const bool is_in = m_variableMap.find(I->getName()) != m_variableMap.end();
            if (is_in) {
                handleInstruction(*I);
                llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaSetSave = m_instructionEncoder.getFormulaSetSave();
                //llvm::outs() << "size: " << formulaSetSave.size();
                //llvm::outs() << "Formula Size Save:" << formulaSetSave.size() << "\n";
                for (SMT::BoolExp *formula : formulaSetSave) {
                    saveVar = satCore->mk_and(saveVar, formula);
                }
                boeDashFirst = satCore->mk_and(boeDashFirst, saveVar);
                boeDash = satCore->mk_and(boeDash, saveVar);
                m_instructionEncoder.resetFormulaSetSave();
            }

            if (llvm::CallInst *C = llvm::dyn_cast<llvm::CallInst>(&I.operator*())) { //handle assume
                bool isAssume =  m_instructionEncoder.getInstName(C).find("assume")<= m_instructionEncoder.getInstName(C).size();
                if (llbmc::FunctionMatcher::isSpecification(*C) || isAssume) {
                    handleInstruction(*I);
                    //llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaSetSave = m_instructionEncoder.getFormulaSetSave();
                    //llvm::outs() << "Formula Size Assume/Call:" << " NA" << "\n";
                    if (m_instructionEncoder.getAssumeBoe() != nullptr) {
                        saveVarAssume = satCore->mk_and(saveVarAssume, m_instructionEncoder.getAssumeBoe());
                        SMT::BoolExp *saveVarAssumeDash = satCore->mk_and(m_instructionEncoder.getAssumeBoeDash(),
                                                                          same());
                        saveVarAssume = satCore->mk_implies(saveVarAssume, saveVarAssumeDash);
                    }
                } else {
                    //different assume or assert
                    //m_instructionEncoder.visit(*C);
                   //TODO testen!! bei einzel branch auch C->getOperand(0)->getName().find("__VERIFIER_nondet");
                    if (!(C->getNumOperands() == 1 && C->getOperand(0)->getName().startswith("__VERIFIER_nondet"))) {
                        //llvm::outs() << " VERIFIER ASSUME\n";
                        m_instructionEncoder.visitVerifierAssume(*C);
                        //llvm::outs() << "Formula Size VERIFIER Assume/Call:" << " NA" << "\n";
                        if (m_instructionEncoder.getAssumeBoe() != nullptr) {
                            saveVarAssume = satCore->mk_and(saveVarAssume, m_instructionEncoder.getAssumeBoe());
                            SMT::BoolExp *saveVarAssumeDash = satCore->mk_and(m_instructionEncoder.getAssumeBoeDash(),
                                                                              same());
                            saveVarAssume = satCore->mk_implies(saveVarAssume, saveVarAssumeDash);
                        }
                    }
                }
            }
        }

        //TODO add overFlowChecks
        //llvm::outs() << "CheckForOverflow" << "\n";
        SMT::BoolExp *saveVarOverflow = satCore->mk_true();
        for (SMT::BoolExp *formula :m_instructionEncoder.getFormulaSetOverflow()) {
            // s=entry && overflow --> s'=error
            //llvm::outs() << " Added Check ";
            saveVarOverflow = satCore->mk_and(saveVarOverflow, formula);
        }
        //TODO clean overflowSet

        SMT::BoolExp *addAssume = m_smtContext->getSatCore()->mk_true();
        llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaAssumeCond = m_instructionEncoder.getNegAssumeCond();
        for (SMT::BoolExp *formula : formulaAssumeCond) {
            addAssume = satCore->mk_and(addAssume, formula);
        }
        boeFirst = satCore->mk_and(boeFirst, addAssume);
        boeSecond = satCore->mk_and(boeSecond, addAssume);

        boeDash = satCore->mk_and(boeDash, same());
        boeDashFirst = satCore->mk_and(boeDashFirst, same());
        SMT::BoolExp *firstOperand = satCore->mk_implies(boeFirst, boeDashFirst);
        SMT::BoolExp *secondOperand = satCore->mk_implies(boeSecond, boeDash);
        secondOperand = satCore->mk_and(secondOperand, saveVarAssume);
        secondOperand = satCore->mk_and(secondOperand, saveVarOverflow);

        SMT::BoolExp *formula = satCore->mk_and(firstOperand, secondOperand);
        //llvm::outs() << "\n-------------------------------\n";
        return formula;
    } else if (termInst->getNumOperands() == 1) { //br b1 or return i32 0 //TODO better: B->isUnconditional()
        //if (termInst->mayReturn() && (bb.getName().startswith("return")) || bb.getName().startswith("__VERIFIER")) {
        if (llvm::ReturnInst *R = llvm::dyn_cast<llvm::ReturnInst>(termInst)) {
            //llvm::outs() << "RETURN STATEMENT:\n";
            //s=current and 0 --> sDash = ok
            //s=current and not(0) --> sDash = error
            m_instructionEncoder.visitReturn(*termInst);
            SMT::BoolExp *save = nullptr;
            if (m_instructionEncoder.getFormulaSetSave().size() > 0) {
                save = *m_instructionEncoder.getFormulaSetSave().begin();
            } else {
                SMT::BoolExp *saveVarAssumeDash = satCore->mk_and(m_instructionEncoder.getAssumeBoeDash(), same());
                save = satCore->mk_implies(m_instructionEncoder.getAssumeBoe(), saveVarAssumeDash);
            }
            return save;
        } else {
            //llvm::outs() << "Only Operand:" << termInst->getOperand(0)->getName() << " / ";
            llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaSet = m_instructionEncoder.getFormulaSet();
            SMT::BVExp *bveDash = m_SMTTranslator.createBV("sDash", width);  // s
            SMT::BoolExp *boeDash = m_SMTTranslator.bvAssignValue(bveDash, m_bbmap.at(
                    termInst->getOperand(0)->getName()));//sDash=bb1
            boeDash = satCore->mk_and(boeDash, boePredDash);

            //check instructions before branch jump
            SMT::BoolExp *saveVarAssume = satCore->mk_true();
            SMT::BoolExp *saveVar = satCore->mk_true();
            for (llvm::BasicBlock::reverse_iterator I = bb.rbegin(); I != bb.rend(); ++I) {
                SMT::BoolExp *saveVar = satCore->mk_true();
                //llvm::outs() << "Iterate Reverse:" << I->getName() << "\n";
                if (I->isTerminator()) { continue; }
                const bool is_in = m_variableMap.find(I->getName()) != m_variableMap.end();
                if (is_in) {
                    handleInstruction(*I);
                    llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaSetSave = m_instructionEncoder.getFormulaSetSave();
                    //llvm::outs() << "Formula Size Save Only Operand(" << I->getName() << "):" << formulaSetSave.size() << "\n";
                    for (SMT::BoolExp *formula : formulaSetSave) {
                        saveVar = satCore->mk_and(saveVar, formula);
                    }
                    boeDash = satCore->mk_and(boeDash, saveVar);
                    m_instructionEncoder.resetFormulaSetSave();
                }

                if (llvm::CallInst *C = llvm::dyn_cast<llvm::CallInst>(&I.operator*())) { //handle assume
                    bool isAssume =  m_instructionEncoder.getInstName(C).find("assume")<= m_instructionEncoder.getInstName(C).size();
                    if (llbmc::FunctionMatcher::isSpecification(*C) || isAssume) {
                        // make sure it is assume and not other call
                        handleInstruction(*I);
                        //llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaSetSave = m_instructionEncoder.getFormulaSetSave();
                        //llvm::outs() << "Formula Size Assume/Call Only Operand:" << "NA" << "\n";
                        if (m_instructionEncoder.getAssumeBoe() != nullptr) {
                            saveVarAssume = satCore->mk_and(saveVarAssume, m_instructionEncoder.getAssumeBoe());
                            SMT::BoolExp *saveVarAssumeDash = satCore->mk_and(m_instructionEncoder.getAssumeBoeDash(),
                                                                              same());
                            saveVarAssume = satCore->mk_implies(saveVarAssume, saveVarAssumeDash);
                        }
                    }
                }
            }
            boeDash = satCore->mk_and(boeDash, same());
            SMT::BoolExp *onlyOperand = satCore->mk_implies(boeFirst, boeDash);
            SMT::BoolExp *onlyOperandWithAssume = satCore->mk_and(onlyOperand, saveVarAssume);
            //llvm::outs() << "\n-------------------------------\n";
            return onlyOperandWithAssume;
        }
    } else { //unreachable
        //llvm::outs() << "Unreachable Terminator has to be regarded!\n";
        SMT::BoolExp *aboveUnreachable = satCore->mk_true();
        SMT::BoolExp *saveVar;
        for (llvm::BasicBlock::reverse_iterator I = bb.rbegin(); I != bb.rend(); ++I) {
            if (I->isTerminator()) { continue; }
            handleInstruction(*I);
            llvm::SmallPtrSet<SMT::BoolExp *, 1> formulaSetSave = m_instructionEncoder.getFormulaSetSave();
            //llvm::outs() << "Formula Size Above Unrechable:" << formulaSetSave.size() << "\n";
            if (formulaSetSave.size() < 1) {
                aboveUnreachable = satCore->mk_and(m_instructionEncoder.getAssumeBoeDash(), same());
                aboveUnreachable = satCore->mk_implies(m_instructionEncoder.getAssumeBoe(), aboveUnreachable);
            } else {
                aboveUnreachable = satCore->mk_and(aboveUnreachable, *formulaSetSave.begin());
                //llvm::outs() << "Care untested structure\n";
            }
        }
        //llvm::outs() << "\n-------------------------------\n";
        return aboveUnreachable;
    }
    return nullptr;
}

/*
 * returns a formula that states that all other variables stay unchanged
 */
SMT::BoolExp *Encoder::same() {

    // //std::cout << "Called same\n";
    //s,sDash,pred,predDash and all variables that are contained in firstOperand
    SMT::SatCore *satCore = m_smtContext->getSatCore();
    SMT::BoolExp *boolExp = satCore->mk_true();
    int width = -1;
    std::set<llvm::StringRef> notUsedVar = m_instructionEncoder.getNotUsedVar();
    for (llvm::StringRef str : notUsedVar) {
        /*  //std::cout << "String same: " << str.str() << "\n";
          if (m_testing2 && str.str() == "tmp17") {
              m_testing = true;
              m_testing2 = false;
              //std::cout << "Happend...1";
              continue;
          }
          if (m_testing && str.str() == "tmp17") {
              //std::cout << "Happend...2";
              m_testing = false;
              m_testing3=true;
              continue;
          }*/
        /*if (str.str() == "tmp13" && m_testing3) {
            //std::cout << "Happend...";
            m_testing3=false;
            //continue;
        }*/
        ////llvm::outs() << "NotUsedVar: "<< str.str()<< "\n";
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
    int bitSum = 0;
    int bitSPred = 0;
    int smtBV = 0;
    llvm::Function *func = m_module.getFunction(string);
    int numBB = func->size();
    for (llvm::BasicBlock &BB : *func) {
       //llvm::outs() << BB.getName() << ", ";
        for (llvm::Instruction &I : BB) {
            //llvm::outs() << I.getName() << ", ";
            for (auto &use : I.uses()) {
                //llvm::outs() << "-----" << *(use.getUser()) << "  /";
                //handles BranchInst a litte bit different because you have to look at the firstOperand
                if (llvm::BranchInst *B = llvm::dyn_cast<llvm::BranchInst>(use.getUser())) {
                    isUsed = isOnlyUsedInBasicBlock(string, &BB, B->getOperand(0));
                } else if (llvm::CallInst *C = llvm::dyn_cast<llvm::CallInst>(use.getUser())) {
                    //TODO handle llbm_assume, edit FunctionMatcher to recognize assume and then use Operand not output
                    isUsed = isOnlyUsedInBasicBlock(string, &BB, use.getUser());
                } else {
                    isUsed = isOnlyUsedInBasicBlock(string, &BB, use.getUser());
                }
                if (isUsed) {
                    if (llvm::PHINode *P = llvm::dyn_cast<llvm::PHINode>(use.getUser())) {
                        //Operands of phi nodes are always calculated before the basic block is executed and must thus be a state variable
                        //but just if they are a Operand not if they are written to...
                       //llvm::outs() << "PHI: " << I.getName() << ", " << P->getName() << "\n";
                        if (!(P->getName() == I.getName())) {
                            isUsed = false;
                        }
                    }
                }
                //llvm::outs() << "used in BB:" << isUsed << "\n";

                if (!isUsed && I.hasName()) {
                    //add variable to state
                    //llvm::outs() << "   USED!";
                    //llvm::outs() << I.getValueName()->getKey() << "\n";
                    //numBB +2 because there is one "ok" node and one "error" node
                    int width = ceil(log(numBB + 2) / log(2.0));
                    bitSPred = width*2;
                        width = I.getType()->getIntegerBitWidth();
                    m_variableSet.insert(I.getValueName()->getKey());
                    m_variableMap[I.getValueName()->getKey()] = width;
                    //llvm::outs() << I.getValueName()->getKey() << "," << width << "/ ";
                    bitSum += width;
                    smtBV++;
                    break;
                }
            }
        }
    }

    bitSPred = ceil(log(numBB + 2) / log(2.0))*2;
    //llvm::outs() << "\nbv: " << smtBV+2 << "\n";
    //llvm::outs() << "sPred: " << bitSPred << "\n";
    //llvm::outs() << "sum of all bits: " << bitSum;
    m_instructionEncoder.setVariableList(m_variableSet);
    //llvm::outs() << "... finished\n";
}

bool Encoder::isOnlyUsedInBasicBlock(llvm::StringRef string, llvm::BasicBlock *currentBB, llvm::Value *user) {
    bool isUsed;
    llvm::Function *func = m_module.getFunction(string);
    for (llvm::BasicBlock &BB : *func) {
        if (BB.getName() != currentBB->getName()) {
            //TODO isUsedInBasicBlock is buggy, Example: finds x.0 and tmp15 in BasicBlock:bb5
            //isUsed = user->isUsedInBasicBlock(&BB);
            isUsed = isUsedInBasicBlock(string, &BB, user);
            ////llvm::outs() << ":::" << currentBB->getName()<< ", " <<BB.getName()<<", " << isUsed << "\n";
            if (isUsed) {
                return false;
            }
        }
    }
    return true;
}

bool Encoder::isUsedInBasicBlock(llvm::StringRef string, llvm::BasicBlock *BB, llvm::Value *user) {
    bool isUsed;
    //llvm::Function *func = m_module.getFunction(string);
    llvm::Instruction *User = llvm::dyn_cast<llvm::Instruction>(user);
    if (User->getParent() == BB) {
        ////llvm::outs() <<"\nIsUsedInBasicBlock:" << User << ","<< BB->getName() << "\n";
        return true;
    }
    return false;
}

void Encoder::handleInstruction(llvm::Instruction &I) {
    if (llvm::CallInst *C = llvm::dyn_cast<llvm::CallInst>(&I)) {
        handleCallNode(C);
    } else if (llvm::PHINode *P = llvm::dyn_cast<llvm::PHINode>(&I)) {
        //std::cout << "PHINode" << std::endl;
        m_instructionEncoder.visit(I);//TODO need distinction?
    } else if (llvm::ReturnInst *R = llvm::dyn_cast<llvm::ReturnInst>(&I)) {
        //std::cout << "ReturnNode" << std::endl;
    } else {
        m_instructionEncoder.visit(I);
    }
}

void Encoder::handleCallNode(llvm::CallInst *C) {
    assert(C->getValueID() == 68);
    if (llbmc::FunctionMatcher::isCalloc(*C)) {
        //std::cout << "isCalloc" << std::endl;
    } else if (llbmc::FunctionMatcher::isFree(*C)) {
        //std::cout << "isFree" << std::endl;
    } else if (llbmc::FunctionMatcher::isSpecification(*C)) {
        m_instructionEncoder.visit(C);
    } else if (llbmc::FunctionMatcher::isAssert(*C)) {
        //std::cout << "isAssert" << std::endl;
        m_instructionEncoder.visit(C);
    } else if (llbmc::FunctionMatcher::isAssertFail(*C)) {
        //std::cout << "isAssertFail" << std::endl;
        m_instructionEncoder.visit(C);
    } else {
        //std::cout << "Undefined Call" << std::endl;
        bool isAssume =  m_instructionEncoder.getInstName(C).find("assume")<= m_instructionEncoder.getInstName(C).size();
        //we could assume verifier_error... because it is difficult to find out
        if (C->getName().size() > 2) {
            m_instructionEncoder.visit(C);
        } else if(isAssume){
            //std::cout << "Assume Assume" << std::endl;
            m_instructionEncoder.visitAssume(*C);
        }else{
            //std::cout << "Assume Error" << std::endl;
            m_instructionEncoder.visitError(*C);
        }
    }
}

SMT::BoolExp *Encoder::getInitialExp() {
    std::string entryName = "entry";
    //llvm::outs() << entryName << m_bbmap.at(entryName) << "\n";
    //llvm::outs() << "ok:" << m_bbmap.at("ok") << "\n";
    //llvm::outs() << "error:" << m_bbmap.at("error") << "\n";
  //TODO bb to entry care
    //return s = 0 and pred = 0;
    int numBB = m_bbmap.size() + 1;
    int width = ceil(log(numBB) / log(2.0));
    SMT::BVExp *sBv = m_SMTTranslator.createBV("s", width);
    SMT::BoolExp *retS = m_SMTTranslator.bvAssignValue(sBv, m_bbmap.at(entryName));
    //llvm::outs() << "mapAt: " << m_bbmap.at(entryName) << "\n";

    SMT::BVExp *predBv = m_SMTTranslator.createBV("pred", width);
    SMT::BoolExp *retPred = m_SMTTranslator.bvAssignValue(predBv, m_bbmap.at(entryName));
    //llvm::outs() << "mapAt: " << m_bbmap.at(entryName) << "\n";
    SMT::BoolExp *ret = m_smtContext->getSatCore()->mk_and(retS, retPred);

    return ret;
}

SMT::BoolExp *Encoder::getGoalExp() {
    //return s = ok;
    int numBB = m_bbmap.size();
    int width = ceil(log(numBB) / log(2.0));
    SMT::BVExp *sBv = m_SMTTranslator.createBV("s", width);
    SMT::BoolExp *ret = m_SMTTranslator.bvAssignValue(sBv, m_bbmap.at("error"));

    /* SMT::BVExp *sBvTest = m_SMTTranslator.createBV("pred", width);
     SMT::BoolExp *retTest = m_SMTTranslator.bvAssignValue(sBvTest, m_bbmap.at("bb4"));
     SMT::BoolExp *retRet = m_smtContext->getSatCore()->mk_and(ret, retTest);*/

    //retRet = m_smtContext->getSatCore()->mk_and(retRet, retTest2);
    return ret;
}

SMT::BoolExp *Encoder::getUniversalExp() {
    int numBB = m_bbmap.size();
    int width = ceil(log(numBB) / log(2.0));
    SMT::BVExp *sBv = m_SMTTranslator.createBV("s", width);
    SMT::BVExp *th = m_SMTTranslator.createConst(numBB, width);
    //llvm::outs() << "numBB+1: " << numBB << "\n";
    SMT::BoolExp *retS = m_SMTTranslator.compare(sBv, th, llvm::CmpInst::ICMP_ULE);
    sBv = m_SMTTranslator.createBV("pred", width);
    th = m_SMTTranslator.createConst(numBB, width);
    SMT::BoolExp *retPred = m_SMTTranslator.compare(sBv, th, llvm::CmpInst::ICMP_ULE);
    SMT::BoolExp *ret = m_smtContext->getSatCore()->mk_and(retS, retPred);
    return ret;
}

SMT::BoolExp *Encoder::getCompleteTest() {
    SMT::BVExp *sBv = m_SMTTranslator.createBV("test", 2);
    SMT::BVExp *th = m_SMTTranslator.createConst(0, 2);
    SMT::BoolExp *retS = m_SMTTranslator.compare(sBv, th, llvm::CmpInst::ICMP_EQ);
    //SMT::BVExp *sBvVar0 = m_SMTTranslator.createBV("var", 2);
    // SMT::BVExp *thDashVar0 = m_SMTTranslator.createConst(0, 2);
    //SMT::BoolExp *retSDashVar0 = m_SMTTranslator.compare(sBvVar0, thDashVar0, llvm::CmpInst::ICMP_EQ);
    //retS=m_smtContext->getSatCore()->mk_and(retS, retSDashVar0);

    SMT::BVExp *sDashBv = m_SMTTranslator.createBV("testDash", 2);
    SMT::BVExp *thDash = m_SMTTranslator.createConst(1, 2);
    SMT::BoolExp *retSDash = m_SMTTranslator.compare(sDashBv, thDash, llvm::CmpInst::ICMP_EQ);
    SMT::BVExp *sDashBvVar = m_SMTTranslator.createBV("var", 2);
    SMT::BVExp *sDashBvVarDash = m_SMTTranslator.createBV("varDash", 2);
    SMT::BoolExp *retSDashVar = m_SMTTranslator.compare(sDashBvVar, sDashBvVarDash, llvm::CmpInst::ICMP_EQ);
    retSDash = m_smtContext->getSatCore()->mk_and(retSDash, retSDashVar);
    SMT::BoolExp *ret = m_smtContext->getSatCore()->mk_implies(retS, retSDash);

    SMT::BVExp *sBv1 = m_SMTTranslator.createBV("test", 2);
    SMT::BVExp *th1 = m_SMTTranslator.createConst(1, 2);
    SMT::BoolExp *retS1 = m_SMTTranslator.compare(sBv1, th1, llvm::CmpInst::ICMP_EQ);
    SMT::BoolExp *test = m_SMTTranslator.compare(sDashBvVar, th1, llvm::CmpInst::ICMP_UGT);
    retS1 = m_smtContext->getSatCore()->mk_and(retS1, test);
    SMT::BVExp *sDashBv1 = m_SMTTranslator.createBV("testDash", 2);
    SMT::BVExp *thDash1 = m_SMTTranslator.createConst(2, 2);
    SMT::BoolExp *retSDash1 = m_SMTTranslator.compare(sDashBv1, thDash1, llvm::CmpInst::ICMP_EQ);
    SMT::BVExp *sDashBvVar2 = m_SMTTranslator.createBV("varDash", 2);
    SMT::BVExp *thDashVar2 = m_SMTTranslator.createConst(2, 2);
    SMT::BoolExp *retSDashVar2 = m_SMTTranslator.compare(sDashBvVar2, thDashVar2, llvm::CmpInst::ICMP_EQ);
    retSDash1 = m_smtContext->getSatCore()->mk_and(retSDash1, retSDashVar2);
    SMT::BoolExp *ret1 = m_smtContext->getSatCore()->mk_implies(retS1, retSDash1);

    SMT::BoolExp *firstAnd = m_smtContext->getSatCore()->mk_and(ret, ret1);

    SMT::BVExp *sBv1X = m_SMTTranslator.createBV("test", 2);
    SMT::BVExp *th1X = m_SMTTranslator.createConst(1, 2);
    SMT::BoolExp *retS1X = m_SMTTranslator.compare(sBv1X, th1X, llvm::CmpInst::ICMP_EQ);
    SMT::BoolExp *testXX = m_SMTTranslator.compare(th1, sDashBvVar, llvm::CmpInst::ICMP_UGE);
    retS1X = m_smtContext->getSatCore()->mk_and(retS1X, testXX);
    SMT::BoolExp *ret1X = m_smtContext->getSatCore()->mk_implies(retS1X, retSDash1);

    firstAnd = m_smtContext->getSatCore()->mk_and(firstAnd, ret1X);


    SMT::BVExp *sBv2 = m_SMTTranslator.createBV("test", 2);
    SMT::BVExp *th2 = m_SMTTranslator.createConst(2, 2);
    SMT::BoolExp *retS2 = m_SMTTranslator.compare(sBv2, th2, llvm::CmpInst::ICMP_EQ);
    SMT::BVExp *sDashBv2 = m_SMTTranslator.createBV("testDash", 2);
    SMT::BVExp *thDash2 = m_SMTTranslator.createConst(3, 2);
    SMT::BoolExp *retSDash2 = m_SMTTranslator.compare(sDashBv2, thDash2, llvm::CmpInst::ICMP_EQ);
    SMT::BVExp *sDashBvVarTTT = m_SMTTranslator.createBV("varDash", 2);
    SMT::BVExp *thDashVar2TTT = m_SMTTranslator.createConst(3, 2);
    SMT::BoolExp *retSDashVar2TTT = m_SMTTranslator.compare(sDashBvVarTTT, thDashVar2TTT, llvm::CmpInst::ICMP_EQ);
    retSDash2 = m_smtContext->getSatCore()->mk_and(retSDash2, retSDashVar2TTT);
    SMT::BoolExp *ret2 = m_smtContext->getSatCore()->mk_implies(retS2, retSDash2);

    SMT::BVExp *sBv3 = m_SMTTranslator.createBV("test", 2);
    SMT::BVExp *th3 = m_SMTTranslator.createConst(3, 2);
    SMT::BoolExp *retS3 = m_SMTTranslator.compare(sBv3, th3, llvm::CmpInst::ICMP_EQ);
    SMT::BVExp *sDashBv3 = m_SMTTranslator.createBV("testDash", 2);
    SMT::BVExp *thDash3 = m_SMTTranslator.createConst(3, 2);
    SMT::BoolExp *retSDash3 = m_SMTTranslator.compare(sDashBv3, thDash3, llvm::CmpInst::ICMP_EQ);
    SMT::BoolExp *ret3 = m_smtContext->getSatCore()->mk_implies(retS3, retSDash3);

    SMT::BoolExp *secondAnd = m_smtContext->getSatCore()->mk_and(ret2, ret3);
    SMT::BoolExp *finalAnd = m_smtContext->getSatCore()->mk_and(firstAnd, secondAnd);

    return finalAnd;
}

SMT::BoolExp *Encoder::encodeSingleBlock(int width, std::string name) {
    SMT::BVExp *bve = m_SMTTranslator.createBV("s", width);  // s
    SMT::BoolExp *boeFirst = m_SMTTranslator.bvAssignValue(bve, m_bbmap.at(name));  // s = error
    SMT::BVExp *bveDash = m_SMTTranslator.createBV("sDash", width);  // sDash
    SMT::BoolExp *boeSecond = m_SMTTranslator.bvAssignValue(bveDash, m_bbmap.at(name));  // sDash = error
    boeFirst = m_smtContext->getSatCore()->mk_implies(boeFirst, boeSecond);
    return boeFirst;
}

bool Encoder::isVerifierAssume(llvm::CallInst I) {
    //llvm::outs() << I.getName() << ", " << I.getNumOperands() << ", " << I.getOperand(0)->getName() << "\n";
    return false;
}

SMT::BoolExp *Encoder::getCompleteTestGoal() {
    SMT::BVExp *sBv1 = m_SMTTranslator.createBV("a1", 2);
    SMT::BVExp *sBv2 = m_SMTTranslator.createBV("a2", 2);
    SMT::BVExp *test = m_SMTTranslator.add(sBv1, sBv2);
    SMT::BVExp *con = m_SMTTranslator.createConst(0, 2);
    SMT::BoolExp *eq = m_SMTTranslator.compare(test, con, llvm::CmpInst::ICMP_EQ);
    //SMT::BoolExp *ret1 = m_SMTTranslator.smulo(sBv1,sBv2);
    SMT::BoolExp *ret1 = eq;
    return ret1;
}












