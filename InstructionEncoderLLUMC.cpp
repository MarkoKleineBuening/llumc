//
// Created by marko on 04.07.17.
//

#include "InstructionEncoderLLUMC.h"

InstructionEncoderLLUMC::InstructionEncoderLLUMC(llbmc::SMTContext *context)
        : m_SMTTranslator(*context), m_variableSet(), m_formulaSet(), m_formulaSetICMP(), m_formulaNegAssumeCond(),
          m_lastExp(), m_alreadyVisited(), m_notUsedVar(), m_smtContext(context), m_formulaSetSave(),
          m_formulaSetBV(), m_bbmap(), m_assumeBoe(), m_assumeBoeDash(), m_overFlowCheck(){}

void InstructionEncoderLLUMC::visitNonIntrinsicCall(llvm::CallInst &I) {
    llvm::outs() << "--Visit NonIntrinsicCall" << "\n";
    llvm::StringRef name = llbmc::FunctionMatcher::getCalledFunction(I)->getName();
    llvm::StringRef cmd;
    llvm::outs() << name << "\n";
    if (name.startswith("__VERIFIER_")) {       // c commands
        cmd = name.substr(11);
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
        //llvm::outs() << "Unkown specification element encountered(1): " << name.str() << "\n";
    }

    if (cmd.startswith("nondet")) {
        llvm::outs() << "outs: "<<I.getType() << ", " << I.getNumOperands() << ", " << I.getOpcodeName() << "\n";

        // a variable is "created" with nondet just save it as x, does not be repeated here
        llvm::outs() << "m_formulaSetBV was filled with BV" << I.getName() << ", " << I.getType()->getIntegerBitWidth() << "\n";
        m_formulaSetBV = m_SMTTranslator.createBV(I.getName(), I.getType()->getIntegerBitWidth());
        llvm::outs() << "bitvector was created";
    } else if (cmd.startswith("assume")) {
        //llvm::outs() << name.str() << ": ";
        visitAssume(I);
        //llvm::outs() << "\n";
    } else {
        //llvm::outs() << "Unkown specification element encountered(2): " << name.str() << "\n";
    }

}

void InstructionEncoderLLUMC::visitSpecification(llvm::CallInst &I) {
    //llvm::outs() << "--Visit Specification" << "\n";
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
        //llvm::outs() << "Unkown specification element encountered(1): " << name.str() << "\n";
    }

    if (cmd.startswith("nondef")) {
        // a variable is "created" with nondef just save it as x, does not be repeated here
        //llvm::outs() << "m_formulaSetBV was filled with BV " << I.getName() << "\n";
        m_formulaSetBV = m_SMTTranslator.createBV(I.getName(), I.getType()->getIntegerBitWidth());
    } else if (cmd.startswith("assume")) {
        //llvm::outs() << name.str() << ": ";
        visitAssume(I);
        //llvm::outs() << "\n";
    } else {
        //llvm::outs() << "Unkown specification element encountered(2): " << name.str() << "\n";
    }

}

void InstructionEncoderLLUMC::visitVerifierAssume(llvm::CallInst &I) {
    //llvm::outs() << "--visit VERIFIER Assume ";
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //s=3 and tmp12= 0 --> s'= ok
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32; //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;

        for (unsigned int i = 0; i < args; i++) {
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                bv1 = m_SMTTranslator.createConst(iConstant, width);
            } else {
                // value is a variable so we are finished here but we have to evaluate the variable
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                if (is_in) {
                    //TODO check if && !I.isUsedInBasicBlock(I.getParent()) is needed
                    bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                    const bool alreadyVisited =
                            m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                    if (Inst->isUsedInBasicBlock(I.getParent()) && !alreadyVisited) {
                        //llvm::outs() << "Same Block Instruction: " << Inst->getName();
                        this->visit(Inst);
                        bv1 = m_formulaSetBV;
                    }
                }
            }
            if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                const bool alreadyVisited = m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                if (!alreadyVisited) {
                    //llvm::outs() << "!alreadyVisited is used, do you need it?(VERAssume)";
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
        SMT::BoolExp *negAssume = satCore->mk_not(tmp);
        m_formulaNegAssumeCond.insert(negAssume);
        boe = satCore->mk_and(boe, tmp);
        SMT::BoolExp *formula = satCore->mk_implies(boe, boeDash);
        m_assumeBoe = boe;
        m_assumeBoeDash = boeDash;
        //llvm::outs() << "\n";
    }
}

void InstructionEncoderLLUMC::visitAssume(llvm::CallInst &I) {
    //llvm::outs() << "--visit Assume";
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //s=3 and tmp12= 0 --> s'= ok
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32; //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;

        for (unsigned int i = 0; i < args; i++) {
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                bv1 = m_SMTTranslator.createConst(iConstant, width);
            } else {
                // value is a variable so we are finished here but we have to evaluate the variable
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                if (is_in) {
                    //TODO check if && !I.isUsedInBasicBlock(I.getParent()) is needed
                    bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                    const bool alreadyVisited =
                            m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                    if (Inst->isUsedInBasicBlock(I.getParent()) && !alreadyVisited) {
                        //llvm::outs() << "Same Block Instruction: " << Inst->getName();
                        this->visit(Inst);
                        bv1 = m_formulaSetBV;
                    }
                }
            }
            if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                const bool alreadyVisited = m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                if (!alreadyVisited) {
                    //llvm::outs() << "!alreadyVisited is used, do you need it?(Assume)";
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
        SMT::BoolExp *negAssume = satCore->mk_not(tmp);
        m_formulaNegAssumeCond.insert(negAssume);
        boe = satCore->mk_and(boe, tmp);
        SMT::BoolExp *formula = satCore->mk_implies(boe, boeDash);
        m_assumeBoe = boe;
        m_assumeBoeDash = boeDash;
        //llvm::outs() << "\n";
    }
}

void InstructionEncoderLLUMC::visitICmpInst(llvm::ICmpInst &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
       llvm::outs() << "--Visit ICmp" << "\n";
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
                llvm::outs() << "--Constant ICMP--";
                llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                if(constant.isNegative()){
                    llvm::outs() << "Value is negative\n";
                    dConstant = constant.roundToDouble(true);
                }else{
                    dConstant = constant.roundToDouble();
                }
                if(CI->getValue().getBitWidth()>32&&false){
                    llvm::outs() << "bidwidth is over 32 ";
                    dConstant = -3;
                }
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                llvm::outs() << width << "width,";

                int iConstant = (int) dConstant;
                llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                if (i == 0) {
                    bv1 = m_SMTTranslator.createConst(iConstant, width);
                } else {
                    bv2 = m_SMTTranslator.createConst(iConstant, width);
                }
            } else {
                llvm::outs() << "--1: Not Constant ICMP--\n";
                // value is a variable so we are finished here but we have to evaluate the variable
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                llvm::outs() << "--2: Not Constant ICMP--\n";
                if (is_in && !isNameBefore(*I.getParent(), I.getOperand(i), I.getName())) {
                    //TODO check if && !I.isUsedInBasicBlock(I.getParent()) is okay in another form
                    //check if the bv has already been created (look in the map)
                    //if it is in the map take it from there otherwise create own bv
                    llvm::outs() << "--Create BV--";
                    if (i == 0) {
                        bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                    } else {
                        bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                    }
                } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                    llvm::outs() << "--3: Not Constant ICMP--\n";
                    const bool alreadyVisited =
                            m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                    if (Inst->isUsedInBasicBlock(I.getParent()) && !alreadyVisited) {
                        llvm::outs() << "--Visit again ICMP--";
                        this->visit(Inst);
                        if (i == 0) {
                            bv1 = m_formulaSetBV;
                        } else {
                            bv2 = m_formulaSetBV;
                        }
                    }
                } else {
                        llvm::outs() << "else";
                    llvm::outs() << I.getValueID() << ", " << I.getOperand(i)->getName() <<"\n";
                    llvm::outs() << "else 2";
                    std::string name = "";
                    if(I.getOperand(i)->getName() ==""){
                        std::string name = "bitcastValue";
                    }else{
                        std::string name = I.getOperand(i)->getName();
                    }

                    if (i == 0) {

                        bv1 = m_SMTTranslator.createBV(name, width);
                    } else {
                        bv2 = m_SMTTranslator.createBV(name, width);
                    }
                    if(I.getName()=="main()"){
                        llvm::outs() << "Name main";
                    }
                }
                if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                    const bool alreadyVisited =
                            m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                    if (!alreadyVisited && alreadyVisited) { // deactivated
                        //TODO what to do with further created Formula? Extend the Subject with second method.
                        llvm::outs() << "!alreadyVisited is used, do you need it?(ICMP)";
                        this->visit(Inst);
                        //TODO probably add to formula
                        //llvm::outs() << ";;;;ADD to FORMULA but how?;;;;";
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

        SMT::BVExp *one = m_SMTTranslator.createConst(1, 1);
        SMT::BVExp *zero = m_SMTTranslator.createConst(0, 1);
        SMT::BVExp *bvExpCond = m_SMTTranslator.cond(boolExp, one, zero);

        m_formulaSetBV = bvExpCond;
        m_formulaSet.insert(boolExp);
        m_formulaSetICMP.insert(boolExp);

        //llvm::outs() << "\n";
    }
}

void InstructionEncoderLLUMC::visitZExtInst(llvm::ZExtInst &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        //TODO überprüfe ob sext in bv wirklich das richtige macht. kein normales ext sondern zext ist gewollt.
        m_alreadyVisited.insert(I.getName());
        llvm::outs() << "--Visit ZExt" << "\n";
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width; //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;
        SMT::BVExp *bvSmall = nullptr;
        for (unsigned int i = 0; i < args; i++) {
            bool alreadyVisited = false;
            std::string name = I.getOperand(i)->getName();
            llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            llvm::outs() << *I.getSrcTy() << "->";
            llvm::outs() << *I.getDestTy()<<"\n";

            const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
            if (is_in) {
                //TODO Check if && !I.isUsedInBasicBlock(I.getParent()) is needed
                bvSmall = m_SMTTranslator.createBV(I.getOperand(i)->getName(), I.getSrcTy()->getIntegerBitWidth());
                //llvm::outs() << "is_in:" << I.getSrcTy()->getIntegerBitWidth();
                if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                    //llvm::outs() << "!alreadyVisited but earlier";
                    this->visit(Inst);
                }
            } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                if (Inst->isUsedInBasicBlock(I.getParent())) {
                    //llvm::outs() << "Same Block Instruction(ZEXT): " << Inst->getName();
                    this->visit(Inst);
                    bvSmall = m_formulaSetBV;
                    //llvm::outs() << "aus Liste:";
                }else{
                    //llvm::outs() << "Not used in Basic Block\n";
                }
            }else if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << "VALUE: " << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                //llvm::outs()<<"bidwidth: " << CI->getValue().getBitWidth()<< ", ";
                dConstant = constant.roundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                bvSmall = m_SMTTranslator.createConst(iConstant, width);
            }else{
                //llvm::outs() << "Nothing triggered\n";
            }
        }
        llvm::outs() << I.getDestTy()->getIntegerBitWidth();
        bv1 = m_SMTTranslator.zext(bvSmall, I.getDestTy()->getIntegerBitWidth());
        const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
        if (is_in) {
            std::string name = I.getName();
            width=I.getDestTy()->getIntegerBitWidth();
            SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
            SMT::BoolExp *save = m_SMTTranslator.compare(bv1, bvExp, llvm::CmpInst::ICMP_EQ);
            m_formulaSetSave.insert(save);
            m_notUsedVar.erase(I.getName());
        }
        m_formulaSetBV = (bv1);
    }
}

void InstructionEncoderLLUMC::visitSExtInst(llvm::SExtInst &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //llvm::outs() << "--Visit SExt" << "\n";
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32; //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;
        SMT::BVExp *bvSmall = nullptr;
        for (unsigned int i = 0; i < args; i++) {
            bool alreadyVisited = false;
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            //llvm::outs() << *I.getSrcTy() << "->";
            //llvm::outs() << *I.getDestTy();

            const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
            if (is_in) {
                //TODO Check if && !I.isUsedInBasicBlock(I.getParent()) is needed
                bvSmall = m_SMTTranslator.createBV(I.getOperand(i)->getName(), I.getSrcTy()->getIntegerBitWidth());
                //llvm::outs() << "is_in:" << I.getSrcTy()->getIntegerBitWidth();
                if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                    //llvm::outs() << "!alreadyVisited but earlier";
                    this->visit(Inst);
                }
            } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                if (Inst->isUsedInBasicBlock(I.getParent())) {
                    //llvm::outs() << "Same Block Instruction(SEXT): " << Inst->getName();
                    this->visit(Inst);
                    bvSmall = m_formulaSetBV;
                    //llvm::outs() << "aus Liste:";
                }
            }else if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {

                //llvm::outs() << "VALUE: " << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << " "<<width << "width,";
                dConstant = constant.roundToDouble(true);
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                bvSmall = m_SMTTranslator.createConst(iConstant, width);
            }else{
                //llvm::outs() << "Nothing triggered\n";
            }
        }
        //llvm::outs() << I.getDestTy()->getIntegerBitWidth();
        bv1 = m_SMTTranslator.sext(bvSmall, I.getDestTy()->getIntegerBitWidth());
        const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
        if (is_in) {
            std::string name = I.getName();
            width=I.getDestTy()->getIntegerBitWidth();
            SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
            SMT::BoolExp *save = m_SMTTranslator.compare(bv1, bvExp, llvm::CmpInst::ICMP_EQ);
            m_formulaSetSave.insert(save);
            m_notUsedVar.erase(I.getName());
        }
        m_formulaSetBV = (bv1);
    }
}

void InstructionEncoderLLUMC::visitPHINode2(llvm::PHINode &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //llvm::outs() << "--Visit PHI" << "\n";
        //if pred = argument --> tmp = 1
        // else tmp = 2
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32;
        int numBB = I.getParent()->getParent()->size();
        int widthBB = ceil(log(numBB + 2) / log(2.0));
        double dConstant;
        SMT::BVExp *bv1 = nullptr;
        SMT::BVExp *bv2 = nullptr;
        //llvm::outs() << *I.getType() << ": ";
        //llvm::outs() << I.getIncomingBlock(0)->getName() << ", " << I.getIncomingBlock(1)->getName();

        for (unsigned int i = 0; i < args; i++) {
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                if (i == 0) {
                    bv1 = m_SMTTranslator.createConst(iConstant, width);
                } else {
                    bv2 = m_SMTTranslator.createConst(iConstant, width);
                }
            } else {
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                if (is_in) {
                    //todo check && !I.isUsedInBasicBlock(I.getParent())
                    width = I.getType()->getIntegerBitWidth();
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
                        //llvm::outs() << "!alreadyVisited is used, do you need it?(PHI)";
                        this->visit(Inst);
                    }
                }
            }
        }
        SMT::BVExp *pred = m_SMTTranslator.createBV("pred", widthBB);
        std::string nameOfParam1 = I.getIncomingBlock(0)->getName();
        //std::string nameOfParam2 = I.getIncomingBlock(1)->getName();
        SMT::BVExp *firstParam = m_SMTTranslator.createConst(m_bbmap.at(nameOfParam1), widthBB);
        SMT::BoolExp *condition = m_SMTTranslator.compare(pred, firstParam, llvm::CmpInst::ICMP_EQ);
        SMT::BVExp *bvExpCond = m_SMTTranslator.cond(condition, bv1, bv2);

        const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
        if (is_in) {
            std::string name = I.getName();
            SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
            //llvm::outs() << "--------" << name << ", ";
            SMT::BoolExp *save = m_SMTTranslator.compare(bvExp, bvExpCond, llvm::CmpInst::ICMP_EQ);
            //llvm::outs() << "\n------phi: " << m_formulaSetSave.size();
            m_formulaSetSave.insert(save);
            m_notUsedVar.erase(I.getName());
        }

        m_formulaSetBV = (bvExpCond);
        //llvm::outs() << "--END PHI\n";
    }
}

void InstructionEncoderLLUMC::visitPHINode(llvm::PHINode &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //llvm::outs() << "--Visit PHI2" << "\n";
        //if pred = argument --> tmp = 1
        // else tmp = 2
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32;
        int numBB = I.getParent()->getParent()->size();
        int widthBB = ceil(log(numBB + 2) / log(2.0));
        double dConstant;
        std::vector<SMT::BVExp*> bvs;
        //llvm::outs() << *I.getType() << ": ";
        //llvm::outs() << I.getIncomingBlock(0)->getName() << ", " << I.getIncomingBlock(1)->getName();

        for (unsigned int i = 0; i < args; i++) {
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                bvs.push_back(m_SMTTranslator.createConst(iConstant, width));
            } else {
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                if (is_in) {
                    //todo check && !I.isUsedInBasicBlock(I.getParent())
                    width = I.getType()->getIntegerBitWidth();
                    bvs.push_back(m_SMTTranslator.createBV(I.getOperand(i)->getName(), width));
                } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                    if (Inst->isUsedInBasicBlock(I.getParent())) {
                        this->visit(Inst);
                        bvs.push_back(m_formulaSetBV);
                    }
                }
                if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                    const bool alreadyVisited =
                            m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                    if (Inst->getParent() == I.getParent() && !alreadyVisited) {
                        //llvm::outs() << "!alreadyVisited is used, do you need it?(PHI)";
                        this->visit(Inst);
                    }
                }
            }
        }
        //llvm::outs() << "--create Phi Formula--";
        SMT::BVExp *bvExpCond = createPhiFormula(args, bvs, I, widthBB, 0);

        const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
        if (is_in) {
            std::string name = I.getName();
            SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
            //llvm::outs() << "--------" << name << ", ";
            SMT::BoolExp *save = m_SMTTranslator.compare(bvExp, bvExpCond, llvm::CmpInst::ICMP_EQ);
            //llvm::outs() << "\n------phi: " << m_formulaSetSave.size();
            m_formulaSetSave.insert(save);
            m_notUsedVar.erase(I.getName());
        }

        m_formulaSetBV = (bvExpCond);
        //llvm::outs() << "--END PHI\n";
    }
}

SMT::BVExp * InstructionEncoderLLUMC::createPhiFormula(unsigned int args, std::vector<SMT::BVExp *> vector, llvm::PHINode &I, int widthBB, int index) {
    //llvm::outs() << args;
    SMT::BVExp *pred = m_SMTTranslator.createBV("pred", widthBB);
    SMT::BVExp *bvExpCond;
    std::string nameOfParam1 = I.getIncomingBlock(index)->getName();
    SMT::BVExp *firstParam = m_SMTTranslator.createConst(m_bbmap.at(nameOfParam1), widthBB);
    SMT::BoolExp *condition = m_SMTTranslator.compare(pred, firstParam, llvm::CmpInst::ICMP_EQ);
    //llvm::outs() << "index: "<< index << " ";
    //TODO MKB: (... && false)
    if(args==2){
        bvExpCond = m_SMTTranslator.cond(condition, vector[0], vector[1]);
    }else if(index < args-1){
        //llvm::outs() << "--first--";
        return m_SMTTranslator.cond(condition, vector[index], createPhiFormula(args, vector, I, widthBB, index+1));
    }else{
        //llvm::outs() << "--second--";
        return vector[index];
    }
    return bvExpCond;
}

void InstructionEncoderLLUMC::visitAdd(llvm::BinaryOperator &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //llvm::outs() << "--Visit Add" << "\n";
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32; //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;
        SMT::BVExp *bv2 = nullptr;

        for (unsigned int i = 0; i < args; i++) {
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                if (i == 0) {
                    bv1 = m_SMTTranslator.createConst(iConstant, width);
                } else {
                    bv2 = m_SMTTranslator.createConst(iConstant, width);
                }
            } else {
                // value is a variable so we are finished here but we have to evaluate the variable
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                if (is_in && !isNameBefore(*I.getParent(), I.getOperand(i), I.getName())) {
                    //check if the bv has already been created (look in the map)
                    //if it is in the map take it from there otherwise create own bv
                    width = I.getOperand(i)->getType()->getIntegerBitWidth();
                    //llvm::outs() << "width: "<<width<<"\n";
                    if (i == 0) {
                        bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                    } else {
                        bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                    }
                } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                    if (Inst->isUsedInBasicBlock(I.getParent())) {
                        //llvm::outs() << "secondary else block used";
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
                    if (!alreadyVisited) {
                        //llvm::outs() << "!alreadyVisited, sure you need this?(ADD)";
                        //this->visit(Inst);
                    }
                }
            }
        }

        SMT::BVExp *bvAdd = m_SMTTranslator.add(bv1, bv2);
        //TODO Overflow checks for all operations
        checkForOverflow(I,bv1,bv2,"add");

        const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
        if (is_in) {
            std::string name = I.getName();
            SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
            SMT::BoolExp *save = m_SMTTranslator.compare(bvAdd, bvExp, llvm::CmpInst::ICMP_EQ);
            m_formulaSetSave.insert(save);
            m_notUsedVar.erase(I.getName());
        }

        m_formulaSetBV = (bvAdd);
        //llvm::outs() << "--END Add";
        //llvm::outs() << "\n";
    }
}

void InstructionEncoderLLUMC::visitSub(llvm::BinaryOperator &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //llvm::outs() << "--Visit Sub" << "\n";
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32; //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;
        SMT::BVExp *bv2 = nullptr;

        for (unsigned int i = 0; i < args; i++) {
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                if (i == 0) {
                    bv1 = m_SMTTranslator.createConst(iConstant, width);
                } else {
                    bv2 = m_SMTTranslator.createConst(iConstant, width);
                }
            } else {
                // value is a variable so we are finished here but we have to evaluate the variable
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                if (is_in && !isNameBefore(*I.getParent(), I.getOperand(i), I.getName())) {
                    //llvm::outs() << " is in ";
                    //check if the bv has already been created (look in the map)
                    //if it is in the map take it from there otherwise create own bv
                    if (i == 0) {
                        bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                    } else {
                        bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                    }
                } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                    if (Inst->isUsedInBasicBlock(I.getParent())) {
                        //llvm::outs() << "secondary else block used";
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
                    if (!alreadyVisited) {
                        //llvm::outs() << "!alreadyVisited, sure you need this?(ADD)";
                        this->visit(Inst);
                    }
                }
            }
        }

        //SMT::SatCore *satCore = m_smtContext->getSatCore();
        SMT::BVExp *bvSub = m_SMTTranslator.sub(bv1, bv2);
        checkForOverflow(I,bv1,bv2,"sub");
        const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
        if (is_in) {
            std::string name = I.getName();
            SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
            SMT::BoolExp *save = m_SMTTranslator.compare(bvSub, bvExp, llvm::CmpInst::ICMP_EQ);
            m_formulaSetSave.insert(save);
            m_notUsedVar.erase(I.getName());
        }

        m_formulaSetBV = (bvSub);
        //llvm::outs() << "--END Aub";
        //llvm::outs() << "\n";
    }
}

void InstructionEncoderLLUMC::visitOr(llvm::BinaryOperator &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //llvm::outs() << "--Visit Or" << "\n";
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32; //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;
        SMT::BVExp *bv2 = nullptr;

        for (unsigned int i = 0; i < args; i++) {
            bool alreadyVisited = false;
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                if (i == 0) {
                    bv1 = m_SMTTranslator.createConst(iConstant, width);
                } else {
                    bv2 = m_SMTTranslator.createConst(iConstant, width);
                }
            } else {
                // value is a variable so we are finished here but we have to evaluate the variable
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                //TODO Marko: if it is in but in the same block then we have to evaluate it too.
                if (is_in && !isNameBefore(*I.getParent(), I.getOperand(i), I.getName())) {
                    if (i == 0) {
                        bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                    } else {
                        bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                    }
                } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                    if (Inst->isUsedInBasicBlock(I.getParent())) {
                        this->visit(Inst);
                        //llvm::outs() << " not in ";
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
                    if (!alreadyVisited) {
                        //llvm::outs() << "!alreadyVisited, sure you need this?(OR)";
                        this->visit(Inst);
                    }
                }
            }
        }

        SMT::BVExp *bvOr = m_SMTTranslator.doOr(bv1, bv2);

        const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
        if (is_in) {
            //llvm::outs() << " is erased(OR) " << I.getName() << " ";
            std::string name = I.getName();
            SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
            SMT::BoolExp *save = m_SMTTranslator.compare(bvOr, bvExp, llvm::CmpInst::ICMP_EQ);
            //llvm::outs() << "\n------" << m_formulaSetSave.size();
            m_formulaSetSave.insert(save);
            m_notUsedVar.erase(I.getName());
        } else {
            //llvm::outs() << " not in OR ";
        }

        m_formulaSetBV = (bvOr);
        //llvm::outs() << "\n";
    }
}

void InstructionEncoderLLUMC::visitAnd(llvm::BinaryOperator &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //llvm::outs() << "--Visit And" << "\n";
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32; //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;
        SMT::BVExp *bv2 = nullptr;

        for (unsigned int i = 0; i < args; i++) {
            bool alreadyVisited = false;
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                if (i == 0) {
                    bv1 = m_SMTTranslator.createConst(iConstant, width);
                } else {
                    bv2 = m_SMTTranslator.createConst(iConstant, width);
                }
            } else {
                // value is a variable so we are finished here but we have to evaluate the variable
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                //TODO Marko: if it is in but in the same block then we have to evaluate it too.
                if (is_in && !isNameBefore(*I.getParent(), I.getOperand(i), I.getName())) {
                    if (i == 0) {
                        bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                    } else {
                        bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                    }
                } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                    if (Inst->isUsedInBasicBlock(I.getParent())) {
                        this->visit(Inst);
                        //llvm::outs() << " not in ";
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
                    if (!alreadyVisited) {
                        //llvm::outs() << "!alreadyVisited, sure you need this?(OR)";
                        this->visit(Inst);
                    }
                }
            }
        }

        SMT::BVExp *bvAnd = m_SMTTranslator.doAnd(bv1, bv2);

        const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
        if (is_in) {
            //llvm::outs() << " is erased(OR) " << I.getName() << " ";
            std::string name = I.getName();
            SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
            SMT::BoolExp *save = m_SMTTranslator.compare(bvAnd, bvExp, llvm::CmpInst::ICMP_EQ);
            //llvm::outs() << "\n------" << m_formulaSetSave.size();
            m_formulaSetSave.insert(save);
            m_notUsedVar.erase(I.getName());
        } else {
            //llvm::outs() << " not in AND ";
        }

        m_formulaSetBV = (bvAnd);
        //llvm::outs() << "\n";
    }
}

void InstructionEncoderLLUMC::visitMul(llvm::BinaryOperator &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //llvm::outs() << "--Visit Mul" << "\n";
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32; //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;
        SMT::BVExp *bv2 = nullptr;

        for (unsigned int i = 0; i < args; i++) {
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                if (i == 0) {
                    bv1 = m_SMTTranslator.createConst(iConstant, width);
                } else {
                    bv2 = m_SMTTranslator.createConst(iConstant, width);
                }
            } else {
                width = I.getOperand(i)->getType()->getIntegerBitWidth();
                // value is a variable so we are finished here but we have to evaluate the variable
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                if (is_in && !isNameBefore(*I.getParent(), I.getOperand(i), I.getName())) {
                    //llvm::outs() << " is in ";
                    //check if the bv has already been created (look in the map)
                    //if it is in the map take it from there otherwise create own bv
                    if (i == 0) {
                        bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                    } else {
                        bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                    }
                } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                    if (Inst->isUsedInBasicBlock(I.getParent())) {
                        if (!isVerifierCall(Inst)) {
                            this->visit(Inst);
                            //alreadyVisited = true;
                            //llvm::outs() << "secondary else block used";
                            //TODO set m_lastExp or check if only the first element is ever used
                            if (i == 0) {
                                bv1 = m_formulaSetBV;
                            } else {
                                bv2 = m_formulaSetBV;
                            }
                        } else {
                            if (i == 0) {
                                bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                            } else {
                                bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                            }
                        }
                    }

                }
                /* if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                     const bool alreadyVisited =
                             m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                     if (!alreadyVisited) {
                         //llvm::outs() << "!alreadyVisited, sure you need this?(MUL)";
                         this->visit(Inst);
                     }
                 }*/
            }
        }

        //SMT::SatCore *satCore = m_smtContext->getSatCore();
        SMT::BVExp *bvMul = m_SMTTranslator.mul(bv1, bv2);
        checkForOverflow(I,bv1,bv2,"mul");
        const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
        if (is_in) {
            //llvm::outs() << "Is In in Mul: erase: "<< I.getName().str() << "\n";
            std::string name = I.getName();
            SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
            SMT::BoolExp *save = m_SMTTranslator.compare(bvMul, bvExp, llvm::CmpInst::ICMP_EQ);
            m_formulaSetSave.insert(save);
            m_notUsedVar.erase(I.getName());
        }

        //TODO use this set when needed (currently used only in visitAdd)
        m_formulaSetBV = (bvMul);
        //llvm::outs() << "\n";
    }
}

void InstructionEncoderLLUMC::visitUDiv(llvm::BinaryOperator &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //llvm::outs() << "--Visit UDiv" << "\n";
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32; //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;
        SMT::BVExp *bv2 = nullptr;

        for (unsigned int i = 0; i < args; i++) {
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                if (i == 0) {
                    bv1 = m_SMTTranslator.createConst(iConstant, width);
                } else {
                    bv2 = m_SMTTranslator.createConst(iConstant, width);
                }
            } else {
                width = I.getOperand(i)->getType()->getIntegerBitWidth();
                // value is a variable so we are finished here but we have to evaluate the variable
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                if (is_in && !isNameBefore(*I.getParent(), I.getOperand(i), I.getName())) {
                    //llvm::outs() << " is in ";
                    //check if the bv has already been created (look in the map)
                    //if it is in the map take it from there otherwise create own bv
                    if (i == 0) {
                        bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                    } else {
                        bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                    }
                } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                    if (Inst->isUsedInBasicBlock(I.getParent())) {
                        if (!isVerifierCall(Inst)) {
                            this->visit(Inst);
                            //alreadyVisited = true;
                            //llvm::outs() << "secondary else block used";
                            //TODO set m_lastExp or check if only the first element is ever used
                            if (i == 0) {
                                bv1 = m_formulaSetBV;
                            } else {
                                bv2 = m_formulaSetBV;
                            }
                        } else {
                            if (i == 0) {
                                bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                            } else {
                                bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                            }
                        }
                    }

                }
                /* if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                     const bool alreadyVisited =
                             m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                     if (!alreadyVisited) {
                         //llvm::outs() << "!alreadyVisited, sure you need this?(MUL)";
                         this->visit(Inst);
                     }
                 }*/
            }
        }

        //SMT::SatCore *satCore = m_smtContext->getSatCore();
        SMT::BVExp *bvUDiv = m_SMTTranslator.udiv(bv1, bv2);
        const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
        if (is_in) {
            //llvm::outs() << "Is In in Mul: erase: "<< I.getName().str() << "\n";
            std::string name = I.getName();
            SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
            SMT::BoolExp *save = m_SMTTranslator.compare(bvUDiv, bvExp, llvm::CmpInst::ICMP_EQ);
            m_formulaSetSave.insert(save);
            m_notUsedVar.erase(I.getName());
        }

        //TODO use this set when needed (currently used only in visitAdd)
        m_formulaSetBV = (bvUDiv);
        //llvm::outs() << "\n";
    }
}

void InstructionEncoderLLUMC::visitXor(llvm::BinaryOperator &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //llvm::outs() << "--Visit Xor" << "\n";
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32; //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;
        SMT::BVExp *bv2 = nullptr;

        for (unsigned int i = 0; i < args; i++) {
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                if (i == 0) {
                    bv1 = m_SMTTranslator.createConst(iConstant, width);
                } else {
                    bv2 = m_SMTTranslator.createConst(iConstant, width);
                }
            } else {
                width = I.getOperand(i)->getType()->getIntegerBitWidth();
                // value is a variable so we are finished here but we have to evaluate the variable
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                if (is_in && !isNameBefore(*I.getParent(), I.getOperand(i), I.getName())) {
                    //llvm::outs() << " is in ";
                    //check if the bv has already been created (look in the map)
                    //if it is in the map take it from there otherwise create own bv
                    if (i == 0) {
                        bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                    } else {
                        bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                    }
                } else if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                    if (Inst->isUsedInBasicBlock(I.getParent())) {
                        if (!isVerifierCall(Inst)) {
                            this->visit(Inst);
                            //alreadyVisited = true;
                            //llvm::outs() << "secondary else block used";
                            //TODO set m_lastExp or check if only the first element is ever used
                            if (i == 0) {
                                bv1 = m_formulaSetBV;
                            } else {
                                bv2 = m_formulaSetBV;
                            }
                        } else {
                            if (i == 0) {
                                bv1 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                            } else {
                                bv2 = m_SMTTranslator.createBV(I.getOperand(i)->getName(), width);
                            }
                        }
                    }

                }
                /* if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(I.getOperand(i))) {
                     const bool alreadyVisited =
                             m_alreadyVisited.find(I.getOperand(i)->getName()) != m_alreadyVisited.end();
                     if (!alreadyVisited) {
                         //llvm::outs() << "!alreadyVisited, sure you need this?(MUL)";
                         this->visit(Inst);
                     }
                 }*/
            }
        }

        //SMT::SatCore *satCore = m_smtContext->getSatCore();
        SMT::BVExp *bvXor = m_SMTTranslator.doXor(bv1, bv2);
        const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
        if (is_in) {
            //llvm::outs() << "Is In in Mul: erase: "<< I.getName().str() << "\n";
            std::string name = I.getName();
            SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
            SMT::BoolExp *save = m_SMTTranslator.compare(bvXor, bvExp, llvm::CmpInst::ICMP_EQ);
            m_formulaSetSave.insert(save);
            m_notUsedVar.erase(I.getName());
        }

        //TODO use this set when needed (currently used only in visitAdd)
        m_formulaSetBV = (bvXor);
        //llvm::outs() << "\n";
    }
}

void InstructionEncoderLLUMC::visitSRem(llvm::BinaryOperator &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //llvm::outs() << "--Visit Srem" << "\n";
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32; //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;
        SMT::BVExp *bv2 = nullptr;

        for (unsigned int i = 0; i < args; i++) {
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                if (i == 0) {
                    bv1 = m_SMTTranslator.createConst(iConstant, width);
                } else {
                    bv2 = m_SMTTranslator.createConst(iConstant, width);
                }
            } else {
                // value is a variable so we are finished here but we have to evaluate the variable
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                if (is_in && !isNameBefore(*I.getParent(), I.getOperand(i), I.getName())) {
                    //llvm::outs() << " is in ";
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
                        //llvm::outs() << "secondary else block used";
                        //TODO set m_lastExp or check if only the first element is ever used
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
                    if (!alreadyVisited) {
                        //llvm::outs() << "!alreadyVisited, sure you need this?(MUL)";
                        this->visit(Inst);
                    }
                }
            }
        }

        //SMT::SatCore *satCore = m_smtContext->getSatCore();
        SMT::BVExp *bvSRem = m_SMTTranslator.srem(bv1, bv2);

        const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
        if (is_in) {
            //llvm::outs() << "Is In in Mul: erase: "<< I.getName().str() << "\n";
            std::string name = I.getName();
            SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
            SMT::BoolExp *save = m_SMTTranslator.compare(bvSRem, bvExp, llvm::CmpInst::ICMP_EQ);
            m_formulaSetSave.insert(save);
            m_notUsedVar.erase(I.getName());
        }

        //TODO use this set when needed (currently used only in visitAdd)
        m_formulaSetBV = (bvSRem);
        //llvm::outs() << "\n";
    }
}

void InstructionEncoderLLUMC::visitURem(llvm::BinaryOperator &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //llvm::outs() << "--Visit URem" << "\n";
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32; //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;
        SMT::BVExp *bv2 = nullptr;

        for (unsigned int i = 0; i < args; i++) {
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                if (i == 0) {
                    bv1 = m_SMTTranslator.createConst(iConstant, width);
                } else {
                    bv2 = m_SMTTranslator.createConst(iConstant, width);
                }
            } else {
                // value is a variable so we are finished here but we have to evaluate the variable
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                if (is_in && !isNameBefore(*I.getParent(), I.getOperand(i), I.getName())) {
                    //llvm::outs() << " is in ";
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
                        //llvm::outs() << "secondary else block used";
                        //TODO set m_lastExp or check if only the first element is ever used
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
                    if (!alreadyVisited) {
                        //llvm::outs() << "!alreadyVisited, sure you need this?(MUL)";
                        this->visit(Inst);
                    }
                }
            }
        }

        //SMT::SatCore *satCore = m_smtContext->getSatCore();
        SMT::BVExp *bvURem = m_SMTTranslator.urem(bv1, bv2);

        const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
        if (is_in) {
            //llvm::outs() << "Is In in Mul: erase: "<< I.getName().str() << "\n";
            std::string name = I.getName();
            SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
            SMT::BoolExp *save = m_SMTTranslator.compare(bvURem, bvExp, llvm::CmpInst::ICMP_EQ);
            m_formulaSetSave.insert(save);
            m_notUsedVar.erase(I.getName());
        }

        //TODO use this set when needed (currently used only in visitAdd)
        m_formulaSetBV = (bvURem);
        //llvm::outs() << "\n";
    }
}

void InstructionEncoderLLUMC::visitAShr(llvm::BinaryOperator &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //llvm::outs() << "--Visit AShr" << "\n";
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32; //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;
        SMT::BVExp *bv2 = nullptr;

        for (unsigned int i = 0; i < args; i++) {
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                if (i == 0) {
                    bv1 = m_SMTTranslator.createConst(iConstant, width);
                } else {
                    bv2 = m_SMTTranslator.createConst(iConstant, width);
                }
            } else {
                // value is a variable so we are finished here but we have to evaluate the variable
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                if (is_in && !isNameBefore(*I.getParent(), I.getOperand(i), I.getName())) {
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
                        //llvm::outs() << "secondary else block used";
                        //TODO set m_lastExp or check if only the first element is ever used
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
                    if (!alreadyVisited) {
                        //llvm::outs() << "!alreadyVisited, sure you need this?(OR)";
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
        //llvm::outs() << "\n";
    }
}


void InstructionEncoderLLUMC::visitTruncInst(llvm::TruncInst &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //llvm::outs() << "--Visit Trunc!" << "\n";
        unsigned int args = I.getNumOperands();
        // llvm::outs() << args << ":";
        int width = I.getSrcTy()->getIntegerBitWidth(); //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;
        SMT::BVExp *bv2 = nullptr;

        for (unsigned int i = 0; i < args; i++) {
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                if (i == 0) {
                    bv1 = m_SMTTranslator.createConst(iConstant, width);
                } else {
                    bv2 = m_SMTTranslator.createConst(iConstant, width);
                }
            } else {
                // value is a variable so we are finished here but we have to evaluate the variable
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                if (is_in && !isNameBefore(*I.getParent(), I.getOperand(i), I.getName())) {
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
                        //llvm::outs() << "secondary else block used";
                        //TODO set m_lastExp or check if only the first element is ever used
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
                    if (!alreadyVisited) {
                        //llvm::outs() << "!alreadyVisited, sure you need this?(OR)";
                        this->visit(Inst);
                    }
                }
            }
        }

        //bv1 holds the bitvector that should be truncated
        //TODO get value
        int upper = I.getDestTy()->getIntegerBitWidth();
        SMT::BVExp *bvTrunc = m_SMTTranslator.trunc(bv1, upper-1, 0);
        width = upper;
        const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
        if (is_in) {
            std::string name = I.getName();
            SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", upper);
            SMT::BoolExp *save = m_SMTTranslator.compare(bvTrunc, bvExp, llvm::CmpInst::ICMP_EQ);
            m_formulaSetSave.insert(save);
            m_notUsedVar.erase(I.getName());
        }

        m_formulaSetBV = (bvTrunc);
        //llvm::outs() << "\n";
    }
}

void InstructionEncoderLLUMC::visitLShr(llvm::BinaryOperator &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //llvm::outs() << "--Visit LShr" << "\n";
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32; //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;
        SMT::BVExp *bv2 = nullptr;

        for (unsigned int i = 0; i < args; i++) {
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                if (i == 0) {
                    bv1 = m_SMTTranslator.createConst(iConstant, width);
                } else {
                    bv2 = m_SMTTranslator.createConst(iConstant, width);
                }
            } else {
                // value is a variable so we are finished here but we have to evaluate the variable
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                if (is_in && !isNameBefore(*I.getParent(), I.getOperand(i), I.getName())) {
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
                        //llvm::outs() << "secondary else block used";
                        //TODO set m_lastExp or check if only the first element is ever used
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
                    if (!alreadyVisited) {
                        //llvm::outs() << "!alreadyVisited, sure you need this?(OR)";
                        this->visit(Inst);
                    }
                }
            }
        }

        //SMT::SatCore *satCore = m_smtContext->getSatCore();
        SMT::BVExp *bvLShr = m_SMTTranslator.lshr(bv1, bv2);

        const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
        if (is_in) {
            std::string name = I.getName();
            SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
            SMT::BoolExp *save = m_SMTTranslator.compare(bvLShr, bvExp, llvm::CmpInst::ICMP_EQ);
            m_formulaSetSave.insert(save);
            m_notUsedVar.erase(I.getName());
        }

        m_formulaSetBV = (bvLShr);
        //llvm::outs() << "\n";
    }
}

void InstructionEncoderLLUMC::visitShl(llvm::BinaryOperator &I) {
    const bool alreadyVisited = m_alreadyVisited.find(I.getName()) != m_alreadyVisited.end();
    if (alreadyVisited) {
        //llvm::outs() << "ALREADY VISITED CONTINUE\n";
    } else {
        m_alreadyVisited.insert(I.getName());
        //llvm::outs() << "--Visit Shl" << "\n";
        unsigned int args = I.getNumOperands();
        //llvm::outs() << args << ":";
        int width = 32; //default width
        double dConstant;
        SMT::BVExp *bv1 = nullptr;
        SMT::BVExp *bv2 = nullptr;

        for (unsigned int i = 0; i < args; i++) {
            std::string name = I.getOperand(i)->getName();
            //llvm::outs() << " /// " << *I.getOperand(i) << ", ";
            if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(i))) {
                //llvm::outs() << CI->getValue() << " , ";
                llvm::APInt constant = CI->getValue();
                llvm::IntegerType *it = CI->getType();
                width = it->getIntegerBitWidth();
                //llvm::outs() << width << "width,";
                dConstant = constant.signedRoundToDouble();
                int iConstant = (int) dConstant;
                //llvm::outs() << iConstant << "iConstant,";
                //Create BVExpr with Constant
                if (i == 0) {
                    bv1 = m_SMTTranslator.createConst(iConstant, width);
                } else {
                    bv2 = m_SMTTranslator.createConst(iConstant, width);
                }
            } else {
                // value is a variable so we are finished here but we have to evaluate the variable
                const bool is_in = m_variableSet.find(I.getOperand(i)->getName()) != m_variableSet.end();
                if (is_in && !isNameBefore(*I.getParent(), I.getOperand(i), I.getName())) {
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
                        //llvm::outs() << "secondary else block used";
                        //TODO set m_lastExp or check if only the first element is ever used
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
                    if (!alreadyVisited) {
                        //llvm::outs() << "!alreadyVisited, sure you need this?(OR)";
                        this->visit(Inst);
                    }
                }
            }
        }

        //SMT::SatCore *satCore = m_smtContext->getSatCore();
        SMT::BVExp *bvShl = m_SMTTranslator.shl(bv1, bv2);

        const bool is_in = m_variableSet.find(I.getName()) != m_variableSet.end();
        if (is_in) {
            std::string name = I.getName();
            SMT::BVExp *bvExp = m_SMTTranslator.createBV(name + "Dash", width);
            SMT::BoolExp *save = m_SMTTranslator.compare(bvShl, bvExp, llvm::CmpInst::ICMP_EQ);
            m_formulaSetSave.insert(save);
            m_notUsedVar.erase(I.getName());
        }

        m_formulaSetBV = (bvShl);
        //llvm::outs() << "\n";
    }
}

bool InstructionEncoderLLUMC::isNameBefore(llvm::BasicBlock &bb, llvm::Value *op, llvm::StringRef currentName) {
    if (llvm::CallInst *C = llvm::dyn_cast<llvm::CallInst>(op)) {
        if (C->getOperand(0)->getName() == "__VERIFIER_nondet_int") {
            //llvm::outs() << "isNameBefore: false(Call): " << bb.getName().str() << ", " << C->getName().str() << ", " << currentName.str() << "\n";
            return false;
        }
    }
    if (llvm::Instruction *Inst = llvm::dyn_cast<llvm::Instruction>(op)) {
        for (llvm::BasicBlock::iterator I = bb.begin(); I != bb.end(); ++I) {
            if (I->getName() == currentName.str()) {
                //llvm::outs() << "isNameBefore: false: " << bb.getName().str() << ", " << Inst->getName().str() << ", " << currentName.str() << "\n";
                return false;
            }
            if (I->getName() == Inst->getName()) {
                //llvm::outs() << "isNameBefore: true: " << bb.getName().str() << ", " << Inst->getName().str() << ", " << currentName.str() << "\n";
                return true;
            }
        }
    }
    //llvm::outs() << "isNameBefore: false: " << bb.getName().str() << ", END , " << currentName.str() << "\n";
    return false;
}


void InstructionEncoderLLUMC::visitBranchInst(llvm::BranchInst &I) {
    m_alreadyVisited.insert(I.getName());
    //llvm::outs() << "--Visit Branch" << "\n";
}

void InstructionEncoderLLUMC::visitReturnInst(llvm::ReturnInst &I) {
    m_alreadyVisited.insert(I.getName());
    //llvm::outs() << "--Visit Return" << "\n";
}

void InstructionEncoderLLUMC::visitUnreachableInst(llvm::UnreachableInst &I) {
    m_alreadyVisited.insert(I.getName());
    //llvm::outs() << "--Visit Unreachable" << "\n";

}

void InstructionEncoderLLUMC::visitAssertFail(llvm::CallInst &I) {
    m_alreadyVisited.insert(I.getName());
    //llvm::outs() << "--Visit Assert Fail" << "\n";

    int numBB = I.getParent()->getParent()->size();
    int widthBB = ceil(log(numBB + 2) / log(2.0));

    SMT::SatCore *satCore = m_smtContext->getSatCore();
    SMT::BVExp *bve = m_SMTTranslator.createBV("s", widthBB);  // s
    SMT::BVExp *bveDash = m_SMTTranslator.createBV("sDash", widthBB);  // sDash
    SMT::BoolExp *boe = m_SMTTranslator.bvAssignValue(bve, m_bbmap.at(I.getParent()->getName()));  // s = entry
    SMT::BoolExp *boeDash = m_SMTTranslator.bvAssignValue(bveDash, m_bbmap.at("error"));  // sDash = ok
    SMT::BoolExp *formula = satCore->mk_implies(boe, boeDash);
    //m_formulaSetSave.insert(formula);
    m_assumeBoe = boe;
    m_assumeBoeDash = boeDash;
    //llvm::outs() << "\n";

}

void InstructionEncoderLLUMC::visitError(llvm::CallInst &I) {
    m_alreadyVisited.insert(I.getName());
    //llvm::outs() << "--Visit Error" << "\n";

    int numBB = I.getParent()->getParent()->size();
    int widthBB = ceil(log(numBB + 2) / log(2.0));

    SMT::SatCore *satCore = m_smtContext->getSatCore();
    SMT::BVExp *bve = m_SMTTranslator.createBV("s", widthBB);  // s
    SMT::BVExp *bveDash = m_SMTTranslator.createBV("sDash", widthBB);  // sDash
    SMT::BoolExp *boe = m_SMTTranslator.bvAssignValue(bve, m_bbmap.at(I.getParent()->getName()));  // s = entry
    SMT::BoolExp *boeDash = m_SMTTranslator.bvAssignValue(bveDash, m_bbmap.at("error"));  // sDash = ok
    SMT::BoolExp *formula = satCore->mk_implies(boe, boeDash);
    //m_formulaSetSave.insert(formula);
    //llvm::outs() << I.getParent()->getName() << ",,";
    m_assumeBoe = boe;
    m_assumeBoeDash = boeDash;
    //llvm::outs() << "\n";
}

void InstructionEncoderLLUMC::visitAssert(llvm::CallInst &I) {
    m_alreadyVisited.insert(I.getName());
    //llvm::outs() << "--Visit Assert" << "\n";
}

void InstructionEncoderLLUMC::visitReturn(llvm::TerminatorInst &I) {
    m_alreadyVisited.insert(I.getName());
    //llvm::outs() << "--Visit Return" << "\n";
    SMT::BVExp *bv;
    int val = 0;
    if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(0))) {
        //llvm::outs() << CI->getValue() << " , ";
        llvm::APInt constant = CI->getValue();
        llvm::IntegerType *it = CI->getType();
        int val = (int) constant.signedRoundToDouble();
        //llvm::outs() << "val:" << val;
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
    m_assumeBoe = boe;
    m_assumeBoeDash = boeDash;
    //m_formulaSetSave.insert(formula);
    //llvm::outs() << "\n";
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

llvm::SmallPtrSet<SMT::BoolExp *, 1> InstructionEncoderLLUMC::getNegAssumeCond() {
    return m_formulaNegAssumeCond;
}

void InstructionEncoderLLUMC::resetNotUsedVar() {
    //llvm::outs() << "Called resetNotUsedVar\n";
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
    m_formulaNegAssumeCond.clear();
    //m_alreadyVisited.clear(); //TODO activate after testing
}

void InstructionEncoderLLUMC::resetFormulaSetSave() {
    m_formulaSetSave.clear();
}

SMT::BoolExp *InstructionEncoderLLUMC::getAssumeBoe() {
    return m_assumeBoe;
}

SMT::BoolExp *InstructionEncoderLLUMC::getAssumeBoeDash() {
    return m_assumeBoeDash;
}

llvm::SmallPtrSet<SMT::BoolExp*, 1> InstructionEncoderLLUMC::getFormulaSetOverflow(){
    return m_overFlowCheck;
}

bool InstructionEncoderLLUMC::isVerifierCall(llvm::Instruction *I) {
    if (llvm::CallInst *C = llvm::dyn_cast<llvm::CallInst>(I)) {
        return C->getOperand(0)->getName().find("__VERIFIER_nondet") <= C->getOperand(0)->getName().size();
    }
    return false;
}

void InstructionEncoderLLUMC::checkForBitshift(llvm::BinaryOperator &I, SMT::BVExp *bv1, SMT::BVExp *bv2, std::string operation){

}

void InstructionEncoderLLUMC::checkForOverflow(llvm::BinaryOperator &I, SMT::BVExp *bv1, SMT::BVExp *bv2, std::string operation) {
//TODO activate or deactive overflowChecks
    bool overflowCheckActivated = true; //edit Marko
    if(overflowCheckActivated) {
        //TODO check it
        bool needCheck = true;
        SMT::BoolExp *overflowCheck;
        if (llvm::OverflowingBinaryOperator *op = llvm::dyn_cast<llvm::OverflowingBinaryOperator>(&I)) {
            if (op->hasNoUnsignedWrap()) {
                //llvm::outs() << "  has nuw\n";
                if (operation == "add") {
                    overflowCheck = m_SMTTranslator.uaddo(bv1, bv2);
                } else if (operation == "mul") {
                    overflowCheck = m_SMTTranslator.umulo(bv1, bv2);
                } else if (operation == "sub") {
                    overflowCheck = m_SMTTranslator.usubo(bv1, bv2);
                } else {
                    //llvm::outs() << "NOT IMPLEMENTED OPERATION";
                }
            } else if (op->hasNoSignedWrap()) {
                //llvm::outs() << "  has nsw\n";
                if (operation == "add") {
                    overflowCheck = m_SMTTranslator.saddo(bv1, bv2);
                } else if (operation == "mul") {
                    overflowCheck = m_SMTTranslator.smulo(bv1, bv2);
                } else if (operation == "div") {
                    overflowCheck = m_SMTTranslator.sdivo(bv1, bv2);
                } else if (operation == "sub") {
                    overflowCheck = m_SMTTranslator.ssubo(bv1, bv2);
                } else {
                    //llvm::outs() << "NOT IMPLEMENTED OPERATION";
                }
            } else {
                needCheck = false;
            }
        }
        if (needCheck) {
            //llvm::outs() << " +Add Check+ ";
            int numBB = I.getParent()->getParent()->size();
            int widthBB = ceil(log(numBB + 2) / log(2.0));
            SMT::SatCore *satCore = m_smtContext->getSatCore();

            SMT::BVExp *bve = m_SMTTranslator.createBV("s", widthBB);  // s
            SMT::BVExp *bveDash = m_SMTTranslator.createBV("sDash", widthBB);  // sDash
            SMT::BoolExp *boe = m_SMTTranslator.bvAssignValue(bve, m_bbmap.at(I.getParent()->getName()));  // s = entry
            SMT::BoolExp *boeDash = m_SMTTranslator.bvAssignValue(bveDash, m_bbmap.at("error"));  // sDash = ok
            //s=entry && !overflowCheck --> s'= error
            SMT::BoolExp *negAssume = satCore->mk_not(overflowCheck);
            if (false) {
                m_formulaNegAssumeCond.insert(negAssume);
                boe = satCore->mk_and(boe, overflowCheck);
            } else {
                m_formulaNegAssumeCond.insert(overflowCheck);
                boe = satCore->mk_and(boe, negAssume);
            }
            SMT::BoolExp *formula = satCore->mk_implies(boe, boeDash);
            //llvm::outs() << "Added Check in IE\n";
            m_overFlowCheck.insert(formula);
        }
    }
}

llvm::StringRef InstructionEncoderLLUMC::getInstName(llvm::CallInst *I) {
    llvm::StringRef name = llbmc::FunctionMatcher::getCalledFunction(I)->getName();
    return name;
}



























