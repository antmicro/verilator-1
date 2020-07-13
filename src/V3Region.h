// -*- mode: C++; c-file-style: "cc-mode" -*-

#ifndef _V3REGION_H_
#define _V3REGION_H_ 1

#include "config_build.h"
#include "verilatedos.h"

#include "V3Error.h"
#include "V3Ast.h"

#define REG_ACTIVE      0
#define REG_REACTIVE    4
#define REG_OBSERVED    8

#define REG_ACT         0
#define REG_INACT       1
#define REG_NBA         2

//============================================================================

class V3Region {
public:
    // CONSTRUCTORS
    static void insertRegions(AstNetlist* nodep);
};

#endif  // Guard
