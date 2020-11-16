// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Expression width calculations
//
// Code available from: https://verilator.org
//
//*************************************************************************
//
// Copyright 2003-2020 by Wilson Snyder. This program is free software; you
// can redistribute it and/or modify it under the terms of either the GNU
// Lesser General Public License Version 3 or the Perl Artistic License
// Version 2.0.
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

#include "config_build.h"
#include "verilatedos.h"

#include "V3RandomizeMethod.h"

//######################################################################
// Visitor that marks classes needing a randomize() method

class RandomizeMethodMarkVisitor : public AstNVisitor {
    typedef std::unordered_set<AstClass*> DerivedSet;
    typedef std::unordered_map<AstClass*, DerivedSet> BaseToDerivedMap;

    BaseToDerivedMap m_baseToDerivedMap;  // Mapping from base classes to classes that extend them

    virtual void visit(AstNode* nodep) override { iterateChildren(nodep); }

    virtual void visit(AstClass* nodep) override {
        iterateChildren(nodep);
        if (nodep->extendsp()) {
            // Save pointer to derived class
            auto* basep = nodep->extendsp()->classp();
            m_baseToDerivedMap[basep].insert(nodep);
        }
    }

    virtual void visit(AstMethodCall* nodep) override {
        iterateChildren(nodep);
        if (nodep->name() != "randomize") return;
        if (AstClassRefDType* classRefp = VN_CAST(nodep->fromp()->dtypep(), ClassRefDType)) {
            auto* classp = classRefp->classp();
            classp->user1(true);
            markMembers(classp);
        }
    }

    void markMembers(AstClass* nodep) {
        for (auto* memberp = nodep->stmtsp(); memberp; memberp = memberp->nextp()) {
            // If member is rand and of class type, mark its class
            if (VN_IS(memberp, Var) && VN_CAST(memberp, Var)->isRand()) {
                if (auto* classRefp = VN_CAST(memberp->dtypep(), ClassRefDType)) {
                    auto* classp = classRefp->classp();
                    markMembers(classp);
                    markDerived(classp);
                    classRefp->classp()->user1(true);
                }
            }
        }
    }

    void markDerived(AstClass* nodep) {
        if (m_baseToDerivedMap.find(nodep) != m_baseToDerivedMap.end()) {
            for (auto* classp : m_baseToDerivedMap.at(nodep)) {
                classp->user1(true);
                markMembers(classp);
                markDerived(classp);
            }
        }
    }

public:
    void markAllDerived() {
        for (auto p : m_baseToDerivedMap) {
            if (p.first->user1()) { markDerived(p.first); }
        }
    }
};

//######################################################################
// Visitor that defines a randomize method where needed

class RandomizeMethodVisitor : public AstNVisitor {
    virtual void visit(AstNode* nodep) override { iterateChildren(nodep); }

    virtual void visit(AstClass* nodep) override {
        iterateChildren(nodep);
        if (nodep->user1()) {
            auto* funcp = V3RandomizeMethod::declareIn(nodep);
            auto* fvarp = VN_CAST(funcp->fvarp(), Var);
            funcp->addStmtsp(new AstAssign(
                nodep->fileline(), new AstVarRef(nodep->fileline(), fvarp, VAccess::WRITE),
                new AstConst(nodep->fileline(), AstConst::WidthedValue(), 32, 1)));
            auto* classp = nodep;
            for (auto* classp = nodep; classp;
                 classp = classp->extendsp() ? classp->extendsp()->classp() : nullptr) {
                for (auto* memberp = classp->stmtsp(); memberp; memberp = memberp->nextp()) {
                    auto* memberVarp = VN_CAST(memberp, Var);
                    if (!memberVarp || !memberVarp->isRand()) continue;
                    AstNode* stmtp = nullptr;
                    auto* refp = new AstVarRef(nodep->fileline(), memberVarp, VAccess::WRITE);
                    if (VN_IS(memberp->dtypep()->skipRefp(), BasicDType)) {
                        stmtp = new AstStdRandomize(nodep->fileline(), refp);
                    } else if (auto* classRefp = VN_CAST(memberp->dtypep(), ClassRefDType)) {
                        auto* funcp = V3RandomizeMethod::declareIn(classRefp->classp());
                        auto* callp
                            = new AstMethodCall(nodep->fileline(), refp, "randomize", nullptr);
                        callp->taskp(funcp);
                        callp->dtypeFrom(funcp);
                        stmtp = callp;
                    } else {
                        delete refp;
                        memberp->v3warn(E_UNSUPPORTED,
                                        "Unsupported: random member variables with type "
                                            << memberp->dtypep()->prettyDTypeNameQ());
                    }
                    if (stmtp) {
                        // Although randomize returns int, we know it is 0/1 so can use faster
                        // AstAnd vs AstLogAnd.
                        stmtp = new AstAnd(nodep->fileline(),
                                           new AstVarRef(nodep->fileline(), fvarp, VAccess::READ),
                                           stmtp);
                        auto* assignp = new AstAssign(
                            nodep->fileline(),
                            new AstVarRef(nodep->fileline(), fvarp, VAccess::WRITE), stmtp);
                        funcp->addStmtsp(assignp);
                    }
                }
            }
            nodep->user1(false);
        }
    }
};

//######################################################################
// Randomize method class functions

void V3RandomizeMethod::defineIfNeeded(AstNetlist* nodep) {
    UINFO(2, __FUNCTION__ << ": " << endl);
    {
        RandomizeMethodMarkVisitor prepVisitor;
        prepVisitor.iterate(nodep);
        prepVisitor.markAllDerived();
        RandomizeMethodVisitor visitor;
        visitor.iterate(nodep);
    }
    V3Global::dumpCheckGlobalTree("randomize_method", 0,
                                  v3Global.opt.dumpTreeLevel(__FILE__) >= 3);
}

AstFunc* V3RandomizeMethod::declareIn(AstClass* nodep) {
    auto* funcp = VN_CAST(nodep->findMember("randomize"), Func);
    if (!funcp) {
        auto* dtypep
            = nodep->findBitDType(32, 32, VSigning::SIGNED);  // IEEE says int return of 0/1
        auto* fvarp = new AstVar(nodep->fileline(), AstVarType::MEMBER, "randomize", dtypep);
        fvarp->lifetime(VLifetime::AUTOMATIC);
        fvarp->funcLocal(true);
        fvarp->funcReturn(true);
        fvarp->direction(VDirection::OUTPUT);
        funcp = new AstFunc(nodep->fileline(), "randomize", nullptr, fvarp);
        funcp->dtypep(dtypep);
        funcp->classMethod(true);
        funcp->isVirtual(true);
        nodep->addMembersp(funcp);
        nodep->repairCache();
    }
    return funcp;
}
