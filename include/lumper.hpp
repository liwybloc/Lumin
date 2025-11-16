#ifndef LUMPER_H
#define LUMPER_H

#include "parser.hpp"
#include <memory>

class Lumper {
public:
    explicit Lumper(const std::shared_ptr<ASTNode> &ast);

    void lump(std::string &lumpLoc);
    std::shared_ptr<ASTNode> unlump(const std::string &lumpLoc);

private:
    const std::shared_ptr<ASTNode> &ast;
};

#endif