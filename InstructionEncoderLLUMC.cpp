//
// Created by marko on 04.07.17.
//

#include "InstructionEncoderLLUMC.h"
#include "Formula.h"

InstructionEncoderLLUMC::InstructionEncoderLLUMC(llbmc::SMTContext *context)
        : m_SMTTranslator(*context), m_variableSet(), m_formulaSet(), m_formulaSetICMP(),
          m_lastExp(), m_alreadyVisited(), m_notUsedVar(), m_smtContext(context), m_formulaSetSave(),
          m_formulaSetBV(), m_bbmap() {}

void InstructionEncoderLLUMC::visitSpecification(llvm::CallInst &I) {
    std::cout << "--Visit Specification" << std::endl;
    llvm::StringRef name = llbmc::FunctionMatcher::getCalledFunction(I)->getName();
    llvm::StringRef cmd;
    if (name.startswith("__llbmc_")) {       // c commands
        cmd = name.substr(8);
    } else if (name.startswith("llbmc.")) {
        cmd = name.substr(6);
    } else if (name.startswith("bugbuster.")) {
        // bugbuster commands
        cmd = name.substr(10);
    } else if (name.startswith("llvm.polynx.")) {
        cmd = name.substr(12);
    } else if (name.startswith("_Z")) {
        llvm::StringRef demangled = demangleName(name);
        if (demangled.startswith("__llbmc_")) {
            cmd = demangled.substr(8); // c++ commands
        }
    } else {
        //throw LLBMCException("Unkown specification element encountered: " + name.str());
        std::cout << "Unkown specification element encountered(1): " << name.str() << std::endl;
    }

    if (cmd.startswith("nondef")) {
        // a variable is "created" with nondef just save it as x, does not be repeated here
    } else if (cmd.startswith("assume")) {
        llvm::outs() << name.str() << ": ";
        visitAssume(I);
        //llvm::outs() << "\n";
    } else {
        std::cout << "Unkown specification element encountered(2): " << name.str() << std::endl;
    }

}

void InstructionEncoderLLUMC::visitAssume(llvm::CallInst &I) {
    llvm::outs() << "--visit Assume";
    m_alreadyVisited.insert(I.getName());
    //s=3 and tmp12= 0 --> s'= ok
    unsigned int args = I.getNumOperands();
    llvm::outs() << args << ":";
    int width = 32; //default width
    double dConstant;
    SMT::BVExp *bv1 = nullptr;

    for (unsigned int i = 0; i < args; i++) {
        std::string name = I.getOperand(i)->getName();
        llvm::outs() << " /// " << *I.getOperand(i) << ", ";
        if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
            //llvm::outs() << CI->getValue() << " , ";
            llvm::APInt constant = CI->getValue();
            llvm::IntegerType *it = CI->getType();
            width = it->getIntegerBitWidth();
            llvm::outs() << width << "width,";
            dConstant = constant.signedRoundToDouble();
            int iConstant = (int) dConstant;
            llvm::outs() << iConstant << "iConstant,";
            //Create BVExpr with Constant
            bv1 = m_SMTTranslator.createConst(iConstant, width);
        } else {
            // value is a variable so we are finished here but we have to evaluate the variable
            const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
            if (is_in) {
                bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
            } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                const bool alreadyVisited = m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                if (Inst->isUsedInBasicBlock(I.getParent()) && !alreadyVisited) {
                    llvm::outs() << "Same Block Instruction: " << Inst->getName();
                    this->visit(Inst);
                    bv1 = m_formulaSetBV;
                }
            }
        }
        if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
            const bool alreadyVisited = m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
            if (!alreadyVisited) {
                llvm::outs() << "!alreadyVisited is used, do you need it?(Assume)";
                this->visit(Inst);
            }
        }
    }
    int numBB = I.getParent()->getParent()->size();
    int widthBB = ceil(log(numBB + 2) / log(2.0));

    SMT::SatCore *satCore = m_smtContext->getSatCore();
    SMT::BVExp *bve = m_SMTTranslator.createBV("s", widthBB);  // s
    SMT::BVExp *bveDash = m_SMTTranslator.createBV("sDash", widthBB);  // sDash
    SMT::BoolExp *boe = m_SMTTranslator.bvAssignValue(bve, m_bbmap.at(I.getParent()->getName()));  // s = entry
    SMT::BoolExp *boeDash = m_SMTTranslator.bvAssignValue(bveDash, m_bbmap.at("ok"));  // sDash = ok
    SMT::BVExp *nullConst = m_SMTTranslator.createConst(0, 32);
    SMT::BoolExp *tmp = m_SMTTranslator.compare(bv1, nullConst, llvm::CmpInst::ICMP_EQ);  // tmp = 0
    boe = satCore->mk_and(boe, tmp);
    SMT::BoolExp *formula = satCore->mk_implies(boe, boeDash);
    m_formulaSetSave.insert(formula);
    std::cout << std::endl;
}

void InstructionEncoderLLUMC::visitICmpInst(llvm::ICmpInst &I) {
    m_alreadyVisited.insert(I.getName());
    std::cout << "--Visit ICmp" << std::endl;
    unsigned int args = I.getNumOperands();
    llvm::outs() << args << ":";
    int width = 32; //default width
    double dConstant;
    SMT::BVExp *bv1 = nullptr;
    SMT::BVExp *bv2 = nullptr;

    for (unsigned int i = 0; i < args; i++) {
        std::string name = I.getOperand(i)->getName();
        llvm::outs() << " /// " << *I.getOperand(i) << ", ";
        if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
            //llvm::outs() << CI->getValue() << " , ";
            llvm::APInt constant = CI->getValue();
            llvm::IntegerType *it = CI->getType();
            width = it->getIntegerBitWidth();
            llvm::outs() << width << "width,";
            dConstant = constant.roundToDouble();
            int iConstant = (int) dConstant;
            llvm::outs() << iConstant << "iConstant,";
            //Create BVExpr with Constant
            if (i == 0) {
                bv1 = m_SMTTranslator.createConst(iConstant, width);
            } else {
                bv2 = m_SMTTranslator.createConst(iConstant, width);
            }
        } else {
            // value is a variable so we are finished here but we have to evaluate the variable
            const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
            if (is_in) {
                //check if the bv has already been created (look in the map)
                //if it is in the map take it from there otherwise create own bv
                if (i == 0) {
                    bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                } else {
                    bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                }
            } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                const bool alreadyVisited = m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                if (Inst->isUsedInBasicBlock(I.getParent()) && !alreadyVisited) {
                    this->visit(Inst);
                    if (i == 0) {
                        bv1 = m_formulaSetBV;
                    } else {
                        bv2 = m_formulaSetBV;
                    }
                }
            }
            if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                const bool alreadyVisited = m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                if (!alreadyVisited && alreadyVisited) { // deactivated
                    //TODO what to do with further created Formula? Extend the Subject with second method.
                    llvm::outs() << "!alreadyVisited is used, do you need it?(ICMP)";
                    this->visit(Inst);
                    //TODO probably add to formula
                    llvm::outs() << ";;;;ADD to FORMULA but how?;;;;";
                }
            }
        }
    }
    llvm::CmpInst::Predicate pred = I.getPredicate();
    SMT::BoolExp *boolExp = m_SMTTranslator.compare(bv1, bv2, pred);

    const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
    if (is_in) {
        SMT::SatCore *satCore = m_smtContext->getSatCore();
        // (tmp10Dash = 1)=(icmp)
        std::string name = I.getName().str();
        SMT::BVExp *bve = m_SMTTranslator.createBV(name + "Dash", 1);
        //llvm::outs() << name << ", " << name+"Dash!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
        SMT::BVExp *one = m_SMTTranslator.createConst(1, 1);
        SMT::BoolExp *bveq = m_SMTTranslator.compare(bve, one, llvm::CmpInst::ICMP_EQ);

        SMT::BoolExp *save1 = satCore->mk_implies(bveq, boolExp);
        SMT::BoolExp *save2 = satCore->mk_implies(boolExp, bveq);
        SMT::BoolExp *save = satCore->mk_and(save1, save2);
        m_formulaSetSave.insert(save);
        m_notUsedVar.erase(I.getName());
    }

    m_formulaSet.insert(boolExp);
    m_formulaSetICMP.insert(boolExp);

    //really observer, I do not think so
    Formula *formula = new Formula();
    formula->Notify(boolExp);
    std::cout << std::endl;
}

void InstructionEncoderLLUMC::visitZExtInst(llvm::ZExtInst &I) {
    //TODO überprüfe ob sext in bv wirklich das richtige macht. kein normales ext sondern zext ist gewollt.
    m_alreadyVisited.insert(I.getName());
    std::cout << "--Visit ZExt" << std::endl;
    unsigned int args = I.getNumOperands();
    llvm::outs() << args << ":";
    int width = 32; //default width
    double dConstant;
    SMT::BVExp *bv1 = nullptr;
    SMT::BVExp *bvSmall = nullptr;
    for (unsigned int i = 0; i < args; i++) {
        bool alreadyVisited = false;
        std::string name = I.getOperand(i)->getName();
        llvm::outs() << " /// " << *I.getOperand(i) << ", ";
        llvm::outs() << *I.getSrcTy() << "->";
        llvm::outs() << *I.getDestTy();

        const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
        if (is_in) {
            bvSmall = m_SMTTranslator.createBV(I.getOperand(i)->getName(), I.getSrcTy()->getIntegerBitWidth());
            llvm::outs() << "is_in:" << I.getSrcTy()->getIntegerBitWidth();
            if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                llvm::outs() << "!alreadyVisited but earlier";
                this->visit(Inst);
            }
        } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
            if (Inst->isUsedInBasicBlock(I.getParent())) {
                llvm::outs() << "Same Block Instruction(ZEXT): " << Inst->getName();
                this->visit(Inst);
                bvSmall = m_formulaSetBV;
                llvm::outs() << "aus Liste:";
            }
        }
    }
    llvm::outs() << I.getDestTy()->getIntegerBitWidth();
    bv1 = m_SMTTranslator.zext(bvSmall, I.getDestTy()->getIntegerBitWidth());
    const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
    if (is_in) {
        std::string name = I.getName();
        SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
        SMT::BoolExp *save = m_SMTTranslator.compare(bv1, bvExp, llvm::CmpInst::ICMP_EQ);
        m_formulaSetSave.insert(save);
        m_notUsedVar.erase(I.getName());
    }
    m_formulaSetBV = (bv1);

}

void InstructionEncoderLLUMC::visitPHINode(llvm::PHINode &I) {
    m_alreadyVisited.insert(I.getName());
    std::cout << "--Visit PHI" << std::endl;
    //if pred = argument --> tmp = 1
    // else tmp = 2
    unsigned int args = I.getNumOperands();
    llvm::outs() << args << ":";
    int width = 32;
    int numBB = I.getParent()->getParent()->size();
    int widthBB = ceil(log(numBB + 2) / log(2.0));
    double dConstant;
    SMT::BVExp *bv1 = nullptr;
    SMT::BVExp *bv2 = nullptr;
    llvm::outs() << *I.getType() << ": ";
    llvm::outs() << I.getIncomingBlock(0)->getName() << ", " << I.getIncomingBlock(1)->getName();

    for (unsigned int i = 0; i < args; i++) {
        std::string name = I.getOperand(i)->getName();
        llvm::outs() << " /// " << *I.getOperand(i) << ", ";
        if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
            //llvm::outs() << CI->getValue() << " , ";
            llvm::APInt constant = CI->getValue();
            llvm::IntegerType *it = CI->getType();
            width = it->getIntegerBitWidth();
            llvm::outs() << width << "width,";
            dConstant = constant.signedRoundToDouble();
            int iConstant = (int) dConstant;
            llvm::outs() << iConstant << "iConstant,";
            //Create BVExpr with Constant
            bv1 = m_SMTTranslator.createConst(iConstant, width);
        } else {
            const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
            if (is_in) {
                if (i == 0) {
                    bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                } else {
                    bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                }
            } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                if (Inst->isUsedInBasicBlock(I.getParent())) {
                    this->visit(Inst);
                    if (i == 0) {
                        bv1 = m_formulaSetBV;
                    } else {
                        bv2 = m_formulaSetBV;
                    }
                }
            }
            if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                const bool alreadyVisited =
                        m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                if (Inst->getParent() == I.getParent() && !alreadyVisited) {
                    llvm::outs() << "!alreadyVisited is used, do you need it?(PHI)";
                    this->visit(Inst);
                }
            }
        }
    }
    SMT::BVExp *pred = m_SMTTranslator.createBV("pred", widthBB);
    std::string nameOfParam1 = I.getIncomingBlock(0)->getName();
    std::string nameOfParam2 = I.getIncomingBlock(1)->getName();
    SMT::BVExp *firstParam = m_SMTTranslator.createConst(m_bbmap.at(nameOfParam1), widthBB);
    SMT::BoolExp *condition = m_SMTTranslator.compare(pred, firstParam, llvm::CmpInst::ICMP_EQ);
    SMT::BVExp *bvExpCond = m_SMTTranslator.cond(condition, bv1, bv2);

    const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
    if (is_in) {
        std::string name = I.getName();
        SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
        llvm::outs() << "--------" << name << ", ";
        SMT::BoolExp *save = m_SMTTranslator.compare(bvExp, bvExpCond, llvm::CmpInst::ICMP_EQ);
        llvm::outs() << "\n------phi: " << m_formulaSetSave.size();
        m_formulaSetSave.insert(save);
        m_notUsedVar.erase(I.getName());
    }

    m_formulaSetBV = (bvExpCond);
    llvm::outs() << "--END PHI\n";

}

void InstructionEncoderLLUMC::visitAdd(llvm::BinaryOperator &I) {
    m_alreadyVisited.insert(I.getName());
    std::cout << "--Visit Add" << std::endl;
    unsigned int args = I.getNumOperands();
    llvm::outs() << args << ":";
    int width = 32; //default width
    double dConstant;
    SMT::BVExp *bv1 = nullptr;
    SMT::BVExp *bv2 = nullptr;

    for (unsigned int i = 0; i < args; i++) {
        std::string name = I.getOperand(i)->getName();
        llvm::outs() << " /// " << *I.getOperand(i) << ", ";
        if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
            //llvm::outs() << CI->getValue() << " , ";
            llvm::APInt constant = CI->getValue();
            llvm::IntegerType *it = CI->getType();
            width = it->getIntegerBitWidth();
            llvm::outs() << width << "width,";
            dConstant = constant.signedRoundToDouble();
            int iConstant = (int) dConstant;
            llvm::outs() << iConstant << "iConstant,";
            //Create BVExpr with Constant
            if (i == 0) {
                bv1 = m_SMTTranslator.createConst(iConstant, width);
            } else {
                bv2 = m_SMTTranslator.createConst(iConstant, width);
            }
        } else {
            // value is a variable so we are finished here but we have to evaluate the variable
            const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
            if (is_in) {
                //check if the bv has already been created (look in the map)
                //if it is in the map take it from there otherwise create own bv
                if (i == 0) {
                    bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                } else {
                    bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                }
            } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                if (Inst->isUsedInBasicBlock(I.getParent())) {
                    llvm::outs() << "secondary else block used";
                    this->visit(Inst);
                    if (i == 0) {
                        bv1 = m_formulaSetBV;
                    } else {
                        bv2 = m_formulaSetBV;
                    }
                }
            }
            if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                const bool alreadyVisited = m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                if (!alreadyVisited) {
                    llvm::outs() << "!alreadyVisited, sure you need this?(ADD)";
                    this->visit(Inst);
                }
            }
        }
    }

    //SMT::SatCore *satCore = m_smtContext->getSatCore();
    SMT::BVExp *bvAdd = m_SMTTranslator.add(bv1, bv2);

    const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
    if (is_in) {
        std::string name = I.getName();
        SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
        SMT::BoolExp *save = m_SMTTranslator.compare(bvAdd, bvExp, llvm::CmpInst::ICMP_EQ);
        m_formulaSetSave.insert(save);
        m_notUsedVar.erase(I.getName());
    }

    m_formulaSetBV = (bvAdd);
    llvm::outs() << "--END Add";
    std::cout << std::endl;
}

void InstructionEncoderLLUMC::visitOr(llvm::BinaryOperator &I) {
    m_alreadyVisited.insert(I.getName());
    std::cout << "--Visit Or" << std::endl;
    unsigned int args = I.getNumOperands();
    llvm::outs() << args << ":";
    int width = 32; //default width
    double dConstant;
    SMT::BVExp *bv1 = nullptr;
    SMT::BVExp *bv2 = nullptr;

    for (unsigned int i = 0; i < args; i++) {
        bool alreadyVisited = false;
        std::string name = I.getOperand(i)->getName();
        llvm::outs() << " /// " << *I.getOperand(i) << ", ";
        if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
            //llvm::outs() << CI->getValue() << " , ";
            llvm::APInt constant = CI->getValue();
            llvm::IntegerType *it = CI->getType();
            width = it->getIntegerBitWidth();
            llvm::outs() << width << "width,";
            dConstant = constant.signedRoundToDouble();
            int iConstant = (int) dConstant;
            llvm::outs() << iConstant << "iConstant,";
            //Create BVExpr with Constant
            if (i == 0) {
                bv1 = m_SMTTranslator.createConst(iConstant, width);
            } else {
                bv2 = m_SMTTranslator.createConst(iConstant, width);
            }
        } else {
            // value is a variable so we are finished here but we have to evaluate the variable
            const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
            if (is_in) {
                if (i == 0) {
                    bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                } else {
                    bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                }
            } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                if (Inst->isUsedInBasicBlock(I.getParent())) {
                    this->visit(Inst);
                    llvm::outs() << " not in ";
                    if (i == 0) {
                        bv1 = m_formulaSetBV;
                    } else {
                        bv2 = m_formulaSetBV;
                    }
                }
            }
            if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                const bool alreadyVisited = m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                if (!alreadyVisited) {
                    llvm::outs() << "!alreadyVisited, sure you need this?(OR)";
                    this->visit(Inst);
                }
            }
        }
    }

    SMT::BVExp *bvOr = m_SMTTranslator.doOr(bv1, bv2);

    const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
    if (is_in) {
        std::string name = I.getName();
        SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
        SMT::BoolExp *save = m_SMTTranslator.compare(bvOr, bvExp, llvm::CmpInst::ICMP_EQ);
        llvm::outs() << "\n------" << m_formulaSetSave.size();
        m_formulaSetSave.insert(save);
        m_notUsedVar.erase(I.getName());
    }

    m_formulaSetBV = (bvOr);
    std::cout << std::endl;
}

void InstructionEncoderLLUMC::visitMul(llvm::BinaryOperator &I) {
    m_alreadyVisited.insert(I.getName());
    std::cout << "--Visit Mul" << std::endl;
    unsigned int args = I.getNumOperands();
    llvm::outs() << args << ":";
    int width = 32; //default width
    double dConstant;
    SMT::BVExp *bv1 = nullptr;
    SMT::BVExp *bv2 = nullptr;

    for (unsigned int i = 0; i < args; i++) {
        std::string name = I.getOperand(i)->getName();
        llvm::outs() << " /// " << *I.getOperand(i) << ", ";
        if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
            //llvm::outs() << CI->getValue() << " , ";
            llvm::APInt constant = CI->getValue();
            llvm::IntegerType *it = CI->getType();
            width = it->getIntegerBitWidth();
            llvm::outs() << width << "width,";
            dConstant = constant.signedRoundToDouble();
            int iConstant = (int) dConstant;
            llvm::outs() << iConstant << "iConstant,";
            //Create BVExpr with Constant
            if (i == 0) {
                bv1 = m_SMTTranslator.createConst(iConstant, width);
            } else {
                bv2 = m_SMTTranslator.createConst(iConstant, width);
            }
        } else {
            // value is a variable so we are finished here but we have to evaluate the variable
            const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
            if (is_in) {
                //check if the bv has already been created (look in the map)
                //if it is in the map take it from there otherwise create own bv
                if (i == 0) {
                    bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                } else {
                    bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                }
            } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                if (Inst->isUsedInBasicBlock(I.getParent())) {
                    this->visit(Inst);
                    //alreadyVisited = true;
                    llvm::outs() << "secondary else block used";
                    //TODO set m_lastExp or check if only the first element is ever used
                    if (i == 0) {
                        bv1 = m_formulaSetBV;
                    } else {
                        bv2 = m_formulaSetBV;
                    }
                }

            }
            if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                const bool alreadyVisited = m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                if (!alreadyVisited) {
                    llvm::outs() << "!alreadyVisited, sure you need this?(MUL)";
                    this->visit(Inst);
                }
            }
        }
    }

    //SMT::SatCore *satCore = m_smtContext->getSatCore();
    SMT::BVExp *bvMul = m_SMTTranslator.mul(bv1, bv2);

    const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
    if (is_in) {
        std::string name = I.getName();
        SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
        SMT::BoolExp *save = m_SMTTranslator.compare(bvMul, bvExp, llvm::CmpInst::ICMP_EQ);
        m_formulaSetSave.insert(save);
        m_notUsedVar.erase(I.getName());
    }

    //TODO use this set when needed (currently used only in visitAdd)
    m_formulaSetBV = (bvMul);
    std::cout << std::endl;
}

void InstructionEncoderLLUMC::visitAShr(llvm::BinaryOperator &I) {
    m_alreadyVisited.insert(I.getName());
    std::cout << "--Visit AShr" << std::endl;
    unsigned int args = I.getNumOperands();
    llvm::outs() << args << ":";
    int width = 32; //default width
    double dConstant;
    SMT::BVExp *bv1 = nullptr;
    SMT::BVExp *bv2 = nullptr;

    for (unsigned int i = 0; i < args; i++) {
        std::string name = I.getOperand(i)->getName();
        llvm::outs() << " /// " << *I.getOperand(i) << ", ";
        if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
            //llvm::outs() << CI->getValue() << " , ";
            llvm::APInt constant = CI->getValue();
            llvm::IntegerType *it = CI->getType();
            width = it->getIntegerBitWidth();
            llvm::outs() << width << "width,";
            dConstant = constant.signedRoundToDouble();
            int iConstant = (int) dConstant;
            llvm::outs() << iConstant << "iConstant,";
            //Create BVExpr with Constant
            if (i == 0) {
                bv1 = m_SMTTranslator.createConst(iConstant, width);
            } else {
                bv2 = m_SMTTranslator.createConst(iConstant, width);
            }
        } else {
            // value is a variable so we are finished here but we have to evaluate the variable
            const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
            if (is_in) {
                //check if the bv has already been created (look in the map)
                //if it is in the map take it from there otherwise create own bv
                if (i == 0) {
                    bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                } else {
                    bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                }
            } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                if (Inst->isUsedInBasicBlock(I.getParent())) {
                    this->visit(Inst);
                    //alreadyVisited = true;
                    llvm::outs() << "secondary else block used";
                    //TODO set m_lastExp or check if only the first element is ever used
                    if (i == 0) {
                        bv1 = m_formulaSetBV;
                    } else {
                        bv2 = m_formulaSetBV;
                    }
                }
            }
            if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                const bool alreadyVisited = m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                if (!alreadyVisited) {
                    llvm::outs() << "!alreadyVisited, sure you need this?(OR)";
                    this->visit(Inst);
                }
            }
        }
    }

    //SMT::SatCore *satCore = m_smtContext->getSatCore();
    SMT::BVExp *bvAShr = m_SMTTranslator.ashr(bv1, bv2);

    const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
    if (is_in) {
        std::string name = I.getName();
        SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
        SMT::BoolExp *save = m_SMTTranslator.compare(bvAShr, bvExp, llvm::CmpInst::ICMP_EQ);
        m_formulaSetSave.insert(save);
        m_notUsedVar.erase(I.getName());
    }

    m_formulaSetBV = (bvAShr);
    std::cout << std::endl;
}

void InstructionEncoderLLUMC::visitBranchInst(llvm::BranchInst &I) {
    m_alreadyVisited.insert(I.getName());
    std::cout << "--Visit Branch" << std::endl;
}

void InstructionEncoderLLUMC::visitReturnInst(llvm::ReturnInst &I) {
    m_alreadyVisited.insert(I.getName());
    std::cout << "--Visit Return" << std::endl;
}

void InstructionEncoderLLUMC::visitUnreachableInst(llvm::UnreachableInst &I) {
    m_alreadyVisited.insert(I.getName());
    std::cout << "--Visit Unreachable" << std::endl;

}

void InstructionEncoderLLUMC::visitAssertFail(llvm::CallInst &I) {
    m_alreadyVisited.insert(I.getName());
    std::cout << "--Visit Assert Fail" << std::endl;

    int numBB = I.getParent()->getParent()->size();
    int widthBB = ceil(log(numBB + 2) / log(2.0));

    SMT::SatCore *satCore = m_smtContext->getSatCore();
    SMT::BVExp *bve = m_SMTTranslator.createBV("s", widthBB);  // s
    SMT::BVExp *bveDash = m_SMTTranslator.createBV("sDash", widthBB);  // sDash
    SMT::BoolExp *boe = m_SMTTranslator.bvAssignValue(bve, m_bbmap.at(I.getParent()->getName()));  // s = entry
    SMT::BoolExp *boeDash = m_SMTTranslator.bvAssignValue(bveDash, m_bbmap.at("error"));  // sDash = ok
    SMT::BoolExp *formula = satCore->mk_implies(boe, boeDash);
    m_formulaSetSave.insert(formula);
    std::cout << std::endl;

}

void InstructionEncoderLLUMC::visitAssert(llvm::CallInst &I) {
    m_alreadyVisited.insert(I.getName());
    std::cout << "--Visit Assert" << std::endl;
}

void InstructionEncoderLLUMC::visitReturn(llvm::TerminatorInst &I) {
    m_alreadyVisited.insert(I.getName());
    std::cout << "--Visit Return" << std::endl;
    SMT::BVExp *bv;
    int val = 0;
    if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(0))) {
        llvm::outs() << CI->getValue() << " , ";
        llvm::APInt constant = CI->getValue();
        llvm::IntegerType *it = CI->getType();
        int val = (int) constant.signedRoundToDouble();
        llvm::outs() << "val:" << val;
    }

    int numBB = I.getParent()->getParent()->size();
    int widthBB = ceil(log(numBB + 2) / log(2.0));
    SMT::BoolExp *boeDash;
    SMT::SatCore *satCore = m_smtContext->getSatCore();
    SMT::BVExp *bve = m_SMTTranslator.createBV("s", widthBB);  // s
    SMT::BVExp *bveDash = m_SMTTranslator.createBV("sDash", widthBB);  // sDash
    SMT::BoolExp *boe = m_SMTTranslator.bvAssignValue(bve, m_bbmap.at(I.getParent()->getName()));  // s = entry
    if (val == 0) {
        boeDash = m_SMTTranslator.bvAssignValue(bveDash, m_bbmap.at("ok"));  // sDash = ok
    } else {
        boeDash = m_SMTTranslator.bvAssignValue(bveDash, m_bbmap.at("error"));  // sDash = ok
    }

    SMT::BoolExp *formula = satCore->mk_implies(boe, boeDash);
    m_formulaSetSave.insert(formula);
    std::cout << std::endl;
}

void InstructionEncoderLLUMC::setVariableList(std::set<llvm::StringRef> variableSet) {
    m_variableSet = variableSet;
}

llvm::SmallPtrSet<SMT::BoolExp *, 1> InstructionEncoderLLUMC::getFormulaSet() {
    return m_formulaSet;
}

llvm::SmallPtrSet<SMT::BoolExp *, 1> InstructionEncoderLLUMC::getFormulaSetICMP() {
    return m_formulaSetICMP;
}

llvm::SmallPtrSet<SMT::BoolExp *, 1> InstructionEncoderLLUMC::getFormulaSetSave() {
    return m_formulaSetSave;
}

void InstructionEncoderLLUMC::resetNotUsedVar() {
    m_notUsedVar = m_variableSet;
}

void InstructionEncoderLLUMC::setBBMap(std::map<llvm::StringRef, int> map) {
    m_bbmap = map;
}

std::set<llvm::StringRef> InstructionEncoderLLUMC::getNotUsedVar() {
    return m_notUsedVar;
}

void InstructionEncoderLLUMC::resetFormulaSet() {
    m_formulaSet.clear();
    m_formulaSetICMP.clear();
    m_formulaSetSave.clear();
    //m_alreadyVisited.clear(); //TODO activate after testing
}

void InstructionEncoderLLUMC::resetFormulaSetSave() {
    m_formulaSetSave.clear();
}


















