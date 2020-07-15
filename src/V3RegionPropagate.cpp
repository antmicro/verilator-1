// -*- mode: C++; c-file-style: "cc-mode" -*-

#include "config_build.h"
#include "verilatedos.h"

#include "V3Global.h"
#include "V3RegionPropagate.h"
#include "V3Ast.h"

#include <map>

class RegionPropagateVisitor : public AstNVisitor {
    int m_region;
    bool m_inFunc;
private:
    // VISITORS
    virtual void visit(AstCFunc* nodep) VL_OVERRIDE {
        UINFO(4, "entering: " << nodep << endl);
        m_region = -1;
        m_inFunc = true;
        iterateChildren(nodep);
        m_inFunc = false;
        nodep->regionId(m_region);
        UINFO(4, "done: " << nodep << endl);
    }
    virtual void visit(AstCCall* nodep) VL_OVERRIDE {
        nodep->regionId(nodep->funcp()->regionId());
    }
    virtual void visit(AstNode* nodep) VL_OVERRIDE {
        if (m_inFunc) {
            int region = nodep->regionId();
            if (region != -1)
                region &= 4;
            UINFO(4, "old region: " << m_region << " new node " << nodep << endl);
            UASSERT_OBJ(m_region == -1 || m_region == region || region == -1, nodep, "Expressions from different regions detected in single function");
            m_region = (region == -1) ? m_region : region;
        }
        iterateChildren(nodep);
    }

public:
    // CONSTRUCTORS
    explicit RegionPropagateVisitor(AstNetlist* nodep) {
        m_inFunc = false;
        iterateChildren(nodep);
        V3Global::dumpCheckGlobalTree("region_prop", 0, v3Global.opt.dumpTreeLevel(__FILE__) >= 3);
    }
    virtual ~RegionPropagateVisitor() {}
};

//######################################################################
// Region class functions

void V3RegionPropagate::propagateRegions(AstNetlist* nodep) {
    UINFO(2, __FUNCTION__ << ": " << endl);
    RegionPropagateVisitor visitor(nodep);
}
