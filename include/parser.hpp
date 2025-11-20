#ifndef PARSER_H
#define PARSER_H

#include "lexer.hpp"
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <sstream>
#include <functional>

struct ASTNode {
    enum class Type {
        PROGRAM,
        PRAGMA,

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

        IMPORT_BLOCK,
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

class Parser;
using KwHandler = std::function<std::shared_ptr<ASTNode>(Parser*, int)>;
using KWMAP = std::unordered_map<std::string, KwHandler>;

class Parser {
public:
    explicit Parser(const std::vector<Token> &tokens, std::string fileName)
        : tokens(tokens), fileName(fileName), kwMap(initKwMap()), current(0) {}

    std::shared_ptr<ASTNode> parseProgram();

    void addPragma(const std::shared_ptr<ASTNode>& programNode,
                   const std::vector<Token> &tokens,
                   const std::string &fileName);

    Parser* parent = nullptr;

    void saveParsedFile(const std::string &fileLoc) { 
        if(parent != nullptr) return parent->saveParsedFile(fileLoc);
        parsedFiles.push_back(fileLoc);
    }
    bool isFileParsed(const std::string &fileLoc) const {
        for(const auto &file : parsedFiles) {
            if(file == fileLoc) return true;
        }
        return false;
    }

private:
    const std::vector<Token> &tokens;
    size_t current = 0;

    std::shared_ptr<ASTNode> importBlock, exportBlock;

    std::vector<std::string> parsedFiles;

    std::string fileName;
    KWMAP kwMap;

    const Token &peek(size_t n = 0) const;
    const Token &consume();
    bool match(Token::Type type, int offset = 0);
    Token expectMultiple(const std::vector<Token::Type> &types, const std::string &err);
    Token expect(Token::Type type, const std::string &err, bool doConsume = true);

    void error(const std::string &msg) const;

    std::shared_ptr<ASTNode> parseWithPragma(const std::shared_ptr<ASTNode> &programNode, const std::string &currentFile, const std::vector<Token> &currentTokens);

    std::shared_ptr<ASTNode> parseArrayLiteral();
    std::shared_ptr<ASTNode> parseArrayLiteralIfBracket();
    std::shared_ptr<ASTNode> parseOptionalArraySize(bool &isArray);
    std::shared_ptr<ASTNode> buildSizedArrayDeclareNode(const Token &typeToken, std::shared_ptr<ASTNode> sizeNode, bool isPrimitive);
    std::shared_ptr<ASTNode> buildTypeNodeFromToken(const Token &typeToken);
    std::shared_ptr<ASTNode> parseDeclarationWithTypeAndName(const Token &typeToken, const Token &nameToken, bool isPrimitive, const std::shared_ptr<ASTNode> &arraySize, bool isArray);
    std::shared_ptr<ASTNode> parseOptionalNdarrayShape();
    KWMAP initKwMap();
    std::shared_ptr<ASTNode> parseStatement(int depth);
    std::shared_ptr<ASTNode> parseBlock(int depth);
    std::shared_ptr<ASTNode> parseExpression();
    std::shared_ptr<ASTNode> parsePrimary();
    std::shared_ptr<ASTNode> parseBinaryOp(std::shared_ptr<ASTNode> left, int minPrecedence = 0);
    int getPrecedence(Token::Type type) const;
};

std::string typeToString(ASTNode::Type type);
std::string astToString(const std::shared_ptr<ASTNode>& node, int indent = 0);

#endif