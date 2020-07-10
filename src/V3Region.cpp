// -*- mode: C++; c-file-style: "cc-mode" -*-

#include "config_build.h"
#include "verilatedos.h"

#include "V3Global.h"
#include "V3Region.h"
#include "V3Ast.h"

#include <map>

class RegionVisitor : public AstNVisitor {
private:

    // VISITORS
    virtual void visit(AstModule* nodep) VL_OVERRIDE {
        UINFO(4, " ACTIVE " << nodep << endl);
        iterateChildren(nodep);
    }
    virtual void visit(AstProgram* nodep) VL_OVERRIDE {
        UINFO(4, " REACTIVE " << nodep << endl);
        iterateChildren(nodep);
    }
    virtual void visit(AstNode* nodep) VL_OVERRIDE {
        iterateChildren(nodep);
    }

public:
    // CONSTRUCTORS
    explicit RegionVisitor(AstNetlist* nodep) {
        iterateChildren(nodep);
    }
    virtual ~RegionVisitor() {}
};

//######################################################################
// Region class functions

void V3Region::insertRegions(AstNetlist* nodep) {
    UINFO(2, __FUNCTION__ << ": " << endl);
    RegionVisitor visitor(nodep);
}
