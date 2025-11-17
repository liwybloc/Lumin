#ifndef PARSER_H
#define PARSER_H

#include "lexer.hpp"
#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <sstream>

struct ASTNode {
    enum class Type {
        PROGRAM,
        NUMBER,
        RANGE,
        STRING,
        BOOL,
        IDENTIFIER,
        SELF_REFERENCE,
        BINARY_OP,
        UNARY_OP,
        PRIMITIVE_ASSIGNMENT,
        STRUCT_ASSIGNMENT,
        EXPRESSION_STATEMENT,
        BLOCK,
        CALL,
        IF_STATEMENT,
        ELSE_STATEMENT,
        WHILE_STATEMENT,
        RETURN_STATEMENT,
        FUNCTION,
        FOR_STATEMENT,
        ARRAY_LITERAL,
        ARRAY_ACCESS,
        ARRAY_ASSIGN,
        READ,
        NDARRAY_ASSIGN,
        SIZED_ARRAY_DECLARE,
        STRUCT_DECLARE,
        IMPORT,
    } type;

    uint8_t valueType;
    BinaryOp binopValue;
    std::string strValue;

    std::string retType;

    Primitive primitiveValue = Primitive::NONE;

    std::vector<std::shared_ptr<ASTNode>> children;

    std::shared_ptr<ASTNode> clone() const {
        auto node = std::make_shared<ASTNode>();
        node->type = type;
        node->valueType = valueType;
        node->binopValue = binopValue;
        node->strValue = strValue;
        node->retType = retType;
        node->primitiveValue = primitiveValue;
        for (const auto &child : children) {
            node->children.push_back(child->clone());
        }
        return node;
    }
};

class Parser {
public:
    explicit Parser(const std::vector<Token> &tokens);
    std::shared_ptr<ASTNode> parse();

private:
    const std::vector<Token> &tokens;
    size_t current = 0;

    const Token &peek(size_t n = 0) const;
    const Token &consume();
    bool match(Token::Type type, int offset = 0);
    Token expectMultiple(const std::vector<Token::Type> &types, const std::string &err);
    Token expect(Token::Type type, const std::string &err, bool doConsume = true);

    void error(const std::string &msg) const;

    std::shared_ptr<ASTNode> parseProgram();
    std::shared_ptr<ASTNode> parseArrayLiteral();
    std::shared_ptr<ASTNode> parseArrayLiteralIfBracket();
    std::shared_ptr<ASTNode> parseOptionalArraySize(bool &isArray);
    std::shared_ptr<ASTNode> buildSizedArrayDeclareNode(const Token &typeToken, std::shared_ptr<ASTNode> sizeNode, bool isPrimitive);
    std::shared_ptr<ASTNode> buildTypeNodeFromToken(const Token &typeToken);
    std::shared_ptr<ASTNode> parseDeclarationWithTypeAndName(const Token &typeToken, const Token &nameToken, bool isPrimitive);
    std::shared_ptr<ASTNode> parseStatement();
    std::shared_ptr<ASTNode> parseBlock();
    std::shared_ptr<ASTNode> parseExpression();
    std::shared_ptr<ASTNode> parsePrimary();
    std::shared_ptr<ASTNode> parseBinaryOp(std::shared_ptr<ASTNode> left, int minPrecedence = 0);
    int getPrecedence(Token::Type type) const;
};

std::string typeToString(ASTNode::Type type);
std::string astToString(const std::shared_ptr<ASTNode>& node, int indent = 0);

#endif