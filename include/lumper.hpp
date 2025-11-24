#ifndef LUMPER_H
#define LUMPER_H

#include "parser.hpp"
#include <memory>
#include <string>
#include "executor.hpp"

class Lumper {
public:
    explicit Lumper(const std::shared_ptr<ASTNode> &ast);

    void lump(const std::string &lumpLoc);
    std::shared_ptr<ParsedASTNode> unlump(const std::string &lumpLoc);

private:
    const std::shared_ptr<ASTNode> ast;
};

#endif