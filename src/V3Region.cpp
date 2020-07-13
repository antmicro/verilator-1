// -*- mode: C++; c-file-style: "cc-mode" -*-

#include "config_build.h"
#include "verilatedos.h"

#include "V3Global.h"
#include "V3Region.h"
#include "V3Ast.h"

#include <map>

class RegionVisitor : public AstNVisitor {
    int m_region;
private:

    // VISITORS
    virtual void visit(AstNodeModule* nodep) VL_OVERRIDE {
        UINFO(4, " ACTIVE " << nodep << endl);
        int old_region = m_region;
        m_region = REG_ACTIVE;
        nodep->regionId(m_region);
        iterateChildren(nodep);
        m_region = old_region;
    }
    virtual void visit(AstProgram* nodep) VL_OVERRIDE {
        UINFO(4, " REACTIVE " << nodep << endl);
        int old_region = m_region;
        m_region = REG_REACTIVE;
        nodep->regionId(m_region);
        iterateChildren(nodep);
        m_region = old_region;
    }
    virtual void visit(AstPropClocked* nodep) VL_OVERRIDE {
        UINFO(4, " OBSERVED " << nodep << endl);
        int old_region = m_region;
        m_region = REG_OBSERVED;
        nodep->regionId(m_region);
        iterateChildren(nodep);
        m_region = old_region;
    }
    virtual void visit(AstAssignDly* nodep) VL_OVERRIDE {
        UINFO(4, " DLY " << nodep << endl);
        nodep->regionId(m_region | REG_NBA);
        iterateChildren(nodep);
    }
    virtual void visit(AstNode* nodep) VL_OVERRIDE {
        UINFO(4, " ACT " << nodep << endl);
        nodep->regionId(m_region | REG_ACT);
        iterateChildren(nodep);
    }

public:
    // CONSTRUCTORS
    explicit RegionVisitor(AstNetlist* nodep) {
        m_region = REG_ACTIVE;
        iterateChildren(nodep);
        V3Global::dumpCheckGlobalTree("region", 0, v3Global.opt.dumpTreeLevel(__FILE__) >= 3);
    }
    virtual ~RegionVisitor() {}
};

//######################################################################
// Region class functions

void V3Region::insertRegions(AstNetlist* nodep) {
    UINFO(2, __FUNCTION__ << ": " << endl);
    RegionVisitor visitor(nodep);
}
