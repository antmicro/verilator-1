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
//
//*************************************************************************
// V3RandomizeMethod's Transformations:
//
// Each randomize() method call:
//      Mark class of object on which randomize() is called
// Mark all classes that inherit from previously marked classed
// Mark all classes whose instances are randomized member variables of marked classes
// Each marked class:
//      define a virtual randomize() method that randomizes its random variables
//
//*************************************************************************

#include "config_build.h"
#include "verilatedos.h"

#include "V3RandomizeMethod.h"

//######################################################################
// Visitor that marks classes needing a randomize() method

class RandomizeMethodMarkVisitor final : public AstNVisitor {
private:
    // NODE STATE
    // Cleared on Netlist
    //  AstClass::user1()       -> bool.  Set true to indicate needs randomize processing
    AstUser1InUse m_inuser1;

    typedef std::unordered_set<AstClass*> DerivedSet;
    typedef std::unordered_map<AstClass*, DerivedSet> BaseToDerivedMap;

    BaseToDerivedMap m_baseToDerivedMap;  // Mapping from base classes to classes that extend them

    // METHODS
    VL_DEBUG_FUNC;

    void markMembers(AstClass* nodep) {
        for (auto* classp = nodep; classp;
             classp = classp->extendsp() ? classp->extendsp()->classp() : nullptr) {
            for (auto* memberp = classp->stmtsp(); memberp; memberp = memberp->nextp()) {
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

    void markAllDerived() {
        for (auto p : m_baseToDerivedMap) {
            if (p.first->user1()) markDerived(p.first);
        }
    }

    // VISITORS
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

    virtual void visit(AstNode* nodep) override { iterateChildren(nodep); }

public:
    // CONSTRUCTORS
    explicit RandomizeMethodMarkVisitor(AstNetlist* nodep) {
        iterate(nodep);
        markAllDerived();
    }
    virtual ~RandomizeMethodMarkVisitor() override = default;
};

//######################################################################
// Visitor that defines a randomize method where needed

class RandomizeMethodVisitor final : public AstNVisitor {
private:
    // NODE STATE
    // Cleared on Netlist
    //  AstClass::user1()       -> bool.  Set true to indicate needs randomize processing
    // AstUser1InUse    m_inuser1;      (Allocated for use in RandomizeMethodMarkVisitor)

    // METHODS
    VL_DEBUG_FUNC;

    static AstNodeMath* newStdRandomize(FileLine* fl, AstNodeVarRef* varp, int offset = 0,
                                        AstMemberDType* memberp = nullptr) {
        AstStructDType* structDtp = VN_CAST(
            memberp ? memberp->subDTypep()->skipRefp() : varp->dtypep()->skipRefp(), StructDType);
        if (structDtp) {
            AstNodeMath* randp = nullptr;
            offset += memberp ? memberp->lsb() : 0;
            for (auto* memberp = structDtp->membersp(); memberp;
                 memberp = VN_CAST(memberp->nextp(), MemberDType))
                randp = !randp ? newStdRandomize(fl, varp, offset, memberp)
                               : new AstAnd(
                                   fl, randp,
                                   newStdRandomize(fl, varp->cloneTree(false), offset, memberp));
            return randp;
        } else {
            return new AstStdRandomize(fl, varp, offset, memberp);
        }
    }

    // VISITORS
    virtual void visit(AstClass* nodep) override {
        iterateChildren(nodep);
        if (!nodep->user1()) return;  // Doesn't need randomize, or already processed
        UINFO(9, "Define randomize() for " << nodep << endl);
        auto* funcp = V3RandomizeMethod::newRandomizeFunc(nodep);
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
                if (VN_IS(memberp->dtypep()->skipRefp(), BasicDType)
                    || VN_IS(memberp->dtypep()->skipRefp(), StructDType)) {
                    auto* refp = new AstVarRef(nodep->fileline(), memberVarp, VAccess::WRITE);
                    stmtp = newStdRandomize(nodep->fileline(), refp);
                } else if (auto* classRefp = VN_CAST(memberp->dtypep(), ClassRefDType)) {
                    auto* refp = new AstVarRef(nodep->fileline(), memberVarp, VAccess::WRITE);
                    auto* funcp = V3RandomizeMethod::newRandomizeFunc(classRefp->classp());
                    auto* callp = new AstMethodCall(nodep->fileline(), refp, "randomize", nullptr);
                    callp->taskp(funcp);
                    callp->dtypeFrom(funcp);
                    stmtp = callp;
                } else {
                    memberp->v3warn(E_UNSUPPORTED,
                                    "Unsupported: random member variables with type "
                                        << memberp->dtypep()->skipRefp()->type().ascii());
                }
                if (stmtp) {
                    // Although randomize returns int, we know it is 0/1 so can use faster
                    // AstAnd vs AstLogAnd.
                    stmtp = new AstAnd(nodep->fileline(),
                                       new AstVarRef(nodep->fileline(), fvarp, VAccess::READ),
                                       stmtp);
                    auto* assignp = new AstAssign(
                        nodep->fileline(), new AstVarRef(nodep->fileline(), fvarp, VAccess::WRITE),
                        stmtp);
                    funcp->addStmtsp(assignp);
                }
            }
        }
        nodep->user1(false);
    }

    virtual void visit(AstNode* nodep) override { iterateChildren(nodep); }

public:
    // CONSTRUCTORS
    explicit RandomizeMethodVisitor(AstNetlist* nodep) { iterate(nodep); }
    virtual ~RandomizeMethodVisitor() override = default;
};

//######################################################################
// Randomize method class functions

void V3RandomizeMethod::randomizeNetlist(AstNetlist* nodep) {
    UINFO(2, __FUNCTION__ << ": " << endl);
    {
        RandomizeMethodMarkVisitor markVisitor(nodep);
        RandomizeMethodVisitor visitor(nodep);
    }
    V3Global::dumpCheckGlobalTree("randomize_method", 0,
                                  v3Global.opt.dumpTreeLevel(__FILE__) >= 3);
}

AstFunc* V3RandomizeMethod::newRandomizeFunc(AstClass* nodep) {
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
