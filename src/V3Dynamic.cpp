// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Mark nodes that need dynamic scheduling
//
//*************************************************************************
// V3Dynamic's Transformations:
//
//  Set m_dynamic in AstNodeFTask and AstNodeProcedure nodes that
//  need dynamic scheduling
//
//  To qualify for dynamic scheduling at least one of the following must
//  be true for the node or its subnodes:
//   - code uses mailbox, semaphore or process variables
//     (only if the class was not overridden by user defined class)
//   - task is declared as virtual method
//   - task is DPI imported
//   - task contain delays but was not inlined
//   - task/function contain statements belonging to different regions
//     (applies to stratified scheduler only)
//
//*************************************************************************

#include "config_build.h"
#include "verilatedos.h"

#include "V3Global.h"
#include "V3Dynamic.h"
#include "V3Ast.h"

#include <map>

class DynamicSubtreeVisitor : public AstNVisitor {
private:
    bool m_dynamic;
    // VISITORS
    virtual void visit(AstNode* nodep) override {
        iterateChildren(nodep);
        nodep->dynamic(m_dynamic);
    }

public:
    explicit DynamicSubtreeVisitor(AstNode* nodep, bool dynamic)
        : m_dynamic{dynamic} {
        UINFO(4, " Marking subnodes of " << nodep << " as " << (dynamic ? "DYNAMIC" : "STATIC")
                                         << endl);
        iterateChildren(nodep);
    }
    virtual ~DynamicSubtreeVisitor() {}
};

class DynamicRegionCheckerVisitor : public AstNVisitor {
private:
    VRegion m_region = VRegion::NONE;
    bool m_mixed = false;
    // VISITORS
    virtual void visit(AstNodeStmt* nodep) override {
        if (m_region == VRegion::NONE)
            m_region = nodep->region();
        else if (m_region != nodep->region())
            m_mixed = true;
    }
    virtual void visit(AstNode* nodep) override { iterateChildren(nodep); }

public:
    explicit DynamicRegionCheckerVisitor(AstNodeFTask* nodep) { iterateChildren(nodep); }
    bool isMixed() const { return m_mixed; }
};

class DynamicVisitor : public AstNVisitor {
private:
    bool m_dynamic = false;
    bool m_inTask = false;
    void markAndClear(AstNode* nodep) {
        // DynamicSubtreeVisitor visitor(nodep, m_dynamic);
        nodep->dynamic(m_dynamic);
        m_dynamic = false;
    }
    // VISITORS
    virtual void visit(AstNodeProcedure* nodep) override {  // Initial/Always/Final
        UINFO(4, "Visiting NodeProcedure: " << nodep << endl);
        iterateChildren(nodep);
        markAndClear(nodep);
    }

    virtual void visit(AstNodeFTask* nodep) override {  // Function/Task
        UINFO(4, "Visiting NodeFTask: " << nodep << endl);
        if (VN_IS(nodep, Task)) m_inTask = true;

        if (v3Global.opt.stratifiedScheduler()) {
            DynamicRegionCheckerVisitor visitor(nodep);
            if (visitor.isMixed()) {
                UINFO(4, "Found NodeFTask with mixed regions: " << nodep << endl);
                m_dynamic = true;
            }
        }

        iterateChildren(nodep);

        m_inTask = false;

        if (nodep->isVirtual()) m_dynamic = true;

        markAndClear(nodep);
    }

    virtual void visit(AstNodeFTaskRef* nodep) override {  // Function/Task calls
        UINFO(4, "Visiting NodeFTaskRef: " << nodep << endl);
        iterateChildren(nodep);
        AstNodeFTask* taskp = nodep->taskp();
        m_dynamic = m_dynamic || taskp->dynamic();
        if (taskp->isVirtual()) m_dynamic = true;
        if (taskp->dpiImport()) m_dynamic = true;
    }

    virtual void
    visit(AstVarRef* nodep) override {  // Predefined classes (process/mailbox/semaphore)
        UINFO(4, "Visiting VarRef: " << nodep << endl);
        if (nodep->varScopep()) {
            AstClassRefDType* dtypep = VN_CAST(nodep->varScopep()->dtypep(), ClassRefDType);
            if (dtypep) {
                UINFO(4, "   ClassRefDType: " << dtypep << endl);
                AstClass* classp = dtypep->classp();
                if (classp && classp->isPredefined()) {
                    const string cname = classp->origName();
                    if (cname == "mailbox")
                        m_dynamic = true;
                    else if (cname == "semaphore")
                        m_dynamic = true;
                    else if (cname == "process")
                        m_dynamic = true;
                }
            }
        }
        iterateChildren(nodep);
    }

    virtual void visit(AstDelay* nodep) override {  // Tasks that contain delays
        UINFO(4, "Visiting Delay: " << nodep << endl);
        if (m_inTask) m_dynamic = true;
        iterateChildren(nodep);
    }

    virtual void visit(AstNode* nodep) override { iterateChildren(nodep); }

public:
    // CONSTRUCTORS
    explicit DynamicVisitor(AstNetlist* nodep) {
        iterateChildren(nodep);
        V3Global::dumpCheckGlobalTree("dynamic", 0, v3Global.opt.dumpTreeLevel(__FILE__) >= 3);
    }
    virtual ~DynamicVisitor() {}
};

//######################################################################
// Dynamic class functions

void V3Dynamic::markDynamic(AstNetlist* nodep) {
    UINFO(2, __FUNCTION__ << ": " << endl);
    DynamicVisitor visitor(nodep);
}
