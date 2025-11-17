#ifndef PARSER_UTILS_H
#define PARSER_UTILS_H

#include <string>
#include "parser.hpp"

std::string typeToString(ASTNode::Type type);
std::string astToString(const std::shared_ptr<ASTNode> &node, int indent);

std::shared_ptr<ASTNode> makeNode(ASTNode::Type t = ASTNode::Type::IDENTIFIER, int valueType = 67);

#endif