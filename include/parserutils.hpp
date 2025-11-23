#ifndef PARSER_UTILS_H
#define PARSER_UTILS_H

#include <string>
#include "parser.hpp"

std::string typeToString(Token::Type type);
std::string astToString(const std::shared_ptr<ASTNode> &node, int indent);

std::shared_ptr<ASTNode> makeTypedNode(ASTNode::Type t, int valueType);
std::shared_ptr<ASTNode> makeNode(ASTNode::Type t);

std::string astTypeToString(ASTNode::Type type);

#endif