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
#include <filesystem>

namespace fs = std::filesystem;

struct ASTNode {
    enum class Type {
        PROGRAM,
        PRAGMA,

        INTEGER,
        RANGE,
        STRING,
        CHAR,
        BOOL,
        IDENTIFIER,

        BINARY_OP,
        UNARY_OP,

        PRIMITIVE_ASSIGNMENT,
        NEW_STRUCT,

        BLOCK,
        CALL,
        IF_STATEMENT,
        ELSE_STATEMENT,
        WHILE_STATEMENT,
        RETURN_STATEMENT,

        FUNCTION,
        NATIVE_STATEMENT,
        
        FOR_STATEMENT,
        CONTINUE,
        BREAK,

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
};

class Parser;
using KwHandler = std::function<std::shared_ptr<ASTNode>(Parser*, int)>;
using KWMAP = std::unordered_map<std::string, KwHandler>;

class Parser {
public:
    explicit Parser(const std::vector<Token> &tokens, std::string fileName)
        : tokens(tokens), fileName(fs::path(fileName).filename().string()), kwMap(initKwMap()), current(0) {}

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

    const Token &peek(size_t n = 0) const {
        if (current + n >= tokens.size()) return tokens.back();
        return tokens[current + n];
    }

    const Token &consume(int amount = 1) {
        if(amount == 1) {
            if (current >= tokens.size()) return tokens.back();
            return tokens[current++];
        }
        for(int i=0;i<amount - 1;i++) {
            consume();
        }
        return consume();
    }

    bool match(Token::Type type, int offset = 0) {
        return peek(offset).type == type;
    }
    bool matchMultiple(Token::Type type, int amount) {
        for(int i=0;i<amount;i++) {
            if (!match(type, i)) return false;
        }
        return true;
    }

    Token expectMultiple(const std::vector<Token::Type> &types, const std::string &err) {
        for (const auto &t : types) {
            if (match(t)) return consume();
        }
        error(err);
        return tokens.back();
    }

    Token expect(Token::Type type, const std::string &err, bool doConsume) {
        if (peek().type != type) {
            error(err);
        }
        return doConsume ? consume() : peek();
    }

    void error(const std::string &msg) const {
        throw std::runtime_error(msg + " at " + fileName + ":" + std::to_string(peek().lineIndex) + ":" + std::to_string(peek().colIndex));
    }

    std::shared_ptr<ASTNode> parseWithPragma(const std::shared_ptr<ASTNode> &programNode, const std::string &currentFile, const std::vector<Token> &currentTokens);

    std::shared_ptr<ASTNode> parseArrayLiteral();
    std::shared_ptr<ASTNode> parseArrayLiteralIfBracket();
    std::shared_ptr<ASTNode> parseOptionalArraySize(bool &isArray);
    std::shared_ptr<ASTNode> buildSizedArrayDeclareNode(const Token &typeToken, std::shared_ptr<ASTNode> sizeNode, bool isPrimitive);
    std::shared_ptr<ASTNode> buildTypeNodeFromToken(const Token &typeToken);
    std::shared_ptr<ASTNode> parseDeclarationWithTypeAndName(const Token &typeToken, const Token &nameToken, bool isPrimitive, const std::shared_ptr<ASTNode> &arraySize, bool isArray, bool skipSemicolon);
    // std::shared_ptr<ASTNode> parseStructInitializer(const Token &nameTok, const std::string type);
    std::shared_ptr<ASTNode> parseIdentifier(const Token &ident, bool dataBit);
    std::shared_ptr<ASTNode> parseStatement(int depth, bool dataBit = false);
    std::shared_ptr<ASTNode> parseStatementPre(int depth, bool dataBit);
    std::shared_ptr<ASTNode> parseBlock(int depth);
    std::shared_ptr<ASTNode> parseExpression();
    std::shared_ptr<ASTNode> parseTernary(int minPrec);
    std::shared_ptr<ASTNode> parsePrimary();
    std::shared_ptr<ASTNode> parseBinaryOp(std::shared_ptr<ASTNode> left, int minPrecedence = 0);
    int getPrecedence(Token::Type type) const;
    KWMAP initKwMap() const;
};

std::string typeToString(ASTNode::Type type);
std::string astToString(const std::shared_ptr<ASTNode>& node, int indent = 0);

#endif