#include "parser.hpp"
#include "parserutils.hpp"
#include <stdexcept>
#include <tuple>
#include <fstream>
#include <sstream>
#include <functional>

const Token &Parser::peek(size_t n) const {
    if (current + n >= tokens.size()) return tokens.back();
    return tokens[current + n];
}

const Token &Parser::consume(int amount) {
    if(amount == 1) {
        if (current >= tokens.size()) return tokens.back();
        return tokens[current++];
    }
    for(int i=0;i<amount - 1;i++) {
        consume();
    }
    return consume();
}

bool Parser::match(Token::Type type, int offset) {
    return peek(offset).type == type;
}
bool Parser::matchMultiple(Token::Type type, int amount) {
    for(int i=0;i<amount;i++) {
        if (!match(type, i)) return false;
    }
    return true;
}

Token Parser::expectMultiple(const std::vector<Token::Type> &types, const std::string &err) {
    for (const auto &t : types) {
        if (match(t)) return consume();
    }
    error(err);
    return tokens.back();
}

Token Parser::expect(Token::Type type, const std::string &err, bool doConsume) {
    if (peek().type != type) {
        error(err);
    }
    return doConsume ? consume() : peek();
}

void Parser::error(const std::string &msg) const {
    throw std::runtime_error(msg + " at " + fileName + ":" + std::to_string(peek().lineIndex) + ":" + std::to_string(peek().colIndex));
}

std::string readFileContents(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) throw std::runtime_error("Cannot open file: " + filename);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::shared_ptr<ASTNode> Parser::parseWithPragma(const std::shared_ptr<ASTNode> &programNode,
                                                 const std::string &currentFile,
                                                 const std::vector<Token> &currentTokens) {

    saveParsedFile(currentFile);

    auto pragmaNode = makeTypedNode(ASTNode::Type::PRAGMA, 1);
    pragmaNode->strValue = currentFile;

    Parser tempParser(currentTokens, currentFile);
    tempParser.parent = this;

    tempParser.importBlock = makeTypedNode(ASTNode::Type::IMPORT_BLOCK, 0);
    pragmaNode->children.push_back(tempParser.importBlock);
    tempParser.exportBlock = makeTypedNode(ASTNode::Type::IMPORT_BLOCK, 0);
    pragmaNode->children.push_back(tempParser.exportBlock);

    while (!tempParser.match(Token::Type::END_OF_FILE)) {
        auto stmt = tempParser.parseStatement(0);
        if (stmt != nullptr) pragmaNode->children.push_back(stmt);
    }

    for (const auto &child : tempParser.importBlock->children) {
        if (child->type == ASTNode::Type::STRING &&
            child->strValue.size() > 4 &&
            child->strValue.substr(child->strValue.size() - 4) == ".lum") {

            const std::string &importFile = child->strValue;
            if (!isFileParsed(importFile)) {
                auto fileContents = readFileContents(importFile);
                auto importedTokens = Lexer(fileContents).tokenize();
                addPragma(programNode, importedTokens, importFile);
            }
        }
    }

    return pragmaNode;
}

std::shared_ptr<ASTNode> Parser::parseProgram() {
    auto programNode = makeTypedNode(ASTNode::Type::PROGRAM, 67);
    addPragma(programNode, tokens, fileName);
    return programNode;
}

void Parser::addPragma(const std::shared_ptr<ASTNode> &programNode,
                       const std::vector<Token> &newTokens,
                       const std::string &newFileName) {
    programNode->children.push_back(parseWithPragma(programNode, newFileName, newTokens));
}

std::shared_ptr<ASTNode> Parser::parseArrayLiteral() {
    expect(Token::Type::LBRACKET, "Expected '[' after array declaration", true);
    auto node = makeTypedNode(ASTNode::Type::ARRAY_LITERAL, 1);
    while (!match(Token::Type::RBRACKET)) {
        node->children.push_back(parseExpression());
        if (match(Token::Type::COMMA)) consume();
    }
    expect(Token::Type::RBRACKET, "Expected ']' after array declaration", true);
    return node;
}

std::shared_ptr<ASTNode> Parser::parseArrayLiteralIfBracket() {
    if (!match(Token::Type::LBRACKET)) return nullptr;
    return parseArrayLiteral();
}

std::shared_ptr<ASTNode> Parser::parseOptionalArraySize(bool &isArray) {
    isArray = false;
    if (!match(Token::Type::LBRACKET)) return nullptr;
    consume();
    isArray = true;
    std::shared_ptr<ASTNode> sizeNode = nullptr;
    if (!match(Token::Type::RBRACKET)) {
        sizeNode = parseExpression();
    }
    expect(Token::Type::RBRACKET, "Expected ']' after array declaration", true);
    return sizeNode;
}

std::shared_ptr<ASTNode> Parser::buildSizedArrayDeclareNode(const Token &typeToken, std::shared_ptr<ASTNode> sizeNode, bool isPrimitive) {
    auto sad = makeTypedNode(ASTNode::Type::SIZED_ARRAY_DECLARE, 1);
    if (isPrimitive) sad->primitiveValue = typeToken.primitiveValue;
    sad->strValue = typeToken.value;
    sad->children.push_back(sizeNode);
    return sad;
}

std::shared_ptr<ASTNode> Parser::buildTypeNodeFromToken(const Token &typeToken) {
    auto typeNode = makeTypedNode(ASTNode::Type::IDENTIFIER, 1);
    typeNode->strValue = typeToken.value;
    return typeNode;
}

std::shared_ptr<ASTNode> Parser::parseDeclarationWithTypeAndName(
    const Token &typeToken,
    const Token &nameToken,
    bool isPrimitive,
    const std::shared_ptr<ASTNode> &arraySize,
    bool isArray,
    bool skipSemicolon
) {
    ASTNode::Type nodeType = isPrimitive ? ASTNode::Type::PRIMITIVE_ASSIGNMENT : ASTNode::Type::STRUCT_ASSIGNMENT;
    auto node = makeTypedNode(nodeType, 1);
    node->strValue = nameToken.value;

    if (!isPrimitive) {
        node->children.push_back(buildTypeNodeFromToken(typeToken));
    } else {
        node->primitiveValue = typeToken.primitiveValue;
    }

    if (arraySize != nullptr) node->children.push_back(buildSizedArrayDeclareNode(typeToken, arraySize, isPrimitive));

    if (match(Token::Type::EQUAL)) {
        consume();
        node->children.push_back(parseExpression());
    }

    if(!skipSemicolon) expect(Token::Type::SEMICOLON, "Expected ';' after assignment", true);
    return node;
}

std::shared_ptr<ASTNode> Parser::parseStatement(int depth, bool dataBit) {
    const Token &tok = peek();

    switch (tok.type) {
        case Token::Type::LBRACE:
            return parseBlock(depth + 1);

        case Token::Type::PRIMITIVE: {
            const Token typeTok = consume();
            bool isArray = false;
            std::shared_ptr<ASTNode> arraySize = parseOptionalArraySize(isArray);
            const Token nameTok = expect(Token::Type::IDENTIFIER, "Expected identifier after type", true);
            return parseDeclarationWithTypeAndName(typeTok, nameTok, true, arraySize, isArray, dataBit);
        }

        case Token::Type::IDENTIFIER:
            if(match(Token::Type::EQUAL, 1)) {
                auto nameTok = consume();
                consume();
                auto node = makeTypedNode(ASTNode::Type::PRIMITIVE_ASSIGNMENT, 1);
                auto value = parseExpression();
                node->strValue = nameTok.value;
                node->children.push_back(value);
                expect(Token::Type::SEMICOLON, "Expected ';' after assignment", true);
                return node;
            }
            if (match(Token::Type::LBRACE, 1)) {
                const Token nameTok = consume();
                consume(); // consume {

                std::vector<std::shared_ptr<ASTNode>> ndarrayShape;
                while(!match(Token::Type::RBRACE)) {
                    ndarrayShape.push_back(parseExpression());
                    if(match(Token::Type::COMMA)) consume();
                }
                expect(Token::Type::RBRACE, "Expected '}' after NDArray declaration", true);

                int selfRefLevel = 0;
                if(match(Token::Type::NOT)) {
                    consume();
                    selfRefLevel++;
                    if(match(Token::Type::NOT)) {
                        consume();
                        selfRefLevel++;
                        if(match(Token::Type::NOT)) {
                            throw std::runtime_error("Invalid self-reference level");
                        }
                    }
                }

                std::shared_ptr<ASTNode> initNode = nullptr;
                if (match(Token::Type::EQUAL)) {
                    consume();
                    initNode = parseExpression();
                }

                auto node = makeTypedNode(ASTNode::Type::NDARRAY_ASSIGN, 1);
                node->strValue = nameTok.value;
                auto effNode = makeTypedNode(ASTNode::Type::NUMBER, 1);
                effNode->strValue = std::to_string(selfRefLevel);
                node->children.push_back(effNode);
                for(auto &shape : ndarrayShape) {
                    node->children.push_back(shape);
                }
                if (initNode != nullptr) node->children.push_back(initNode);

                expect(Token::Type::SEMICOLON, "Expected ';' after NDArray assignment", true);
                return node;
            }

            if (match(Token::Type::IDENTIFIER, 1)) {
                const Token typeTok = consume(); // type
                const Token nameTok = consume(); // variable name
                bool isArray = false;
                auto arraySize = parseOptionalArraySize(isArray);
                if (match(Token::Type::EQUAL)) {
                    consume();

                    expect(Token::Type::LBRACE, "Expected '{' after struct declaration", true);

                    auto initNode = makeTypedNode(ASTNode::Type::STRUCT_ASSIGNMENT, 0);
                    initNode->strValue = nameTok.value;
                    auto type = makeTypedNode(ASTNode::Type::STRING, 1);
                    type->strValue = typeTok.value;
                    initNode->children.push_back(type);

                    while (!match(Token::Type::RBRACE)) {
                        if(match(Token::Type::IDENTIFIER) && match(Token::Type::COLON, 1)) {
                            auto assign = makeTypedNode(ASTNode::Type::PRIMITIVE_ASSIGNMENT, 1);
                            assign->strValue = consume().value; // variable name
                            consume(); // :
                            assign->children.push_back(parseExpression()); // value
                            initNode->children.push_back(assign);
                        } else {
                            initNode->children.push_back(parseExpression());
                        }
                        if (match(Token::Type::COMMA)) consume();
                    }

                    expect(Token::Type::RBRACE, "Expected '}' after struct initializer", true);
                    expect(Token::Type::SEMICOLON, "Expected ';' after struct declaration", true);

                    return initNode;
                }
                return parseDeclarationWithTypeAndName(typeTok, nameTok, false, arraySize, isArray, dataBit);
            }
            break;

        case Token::Type::KEYWORD: {
            const std::string kw = consume().value;

            auto it = kwMap.find(kw);
            if (it == kwMap.end())
                error("Unexpected keyword: " + kw);

            return it->second(this, depth);
        }

        default:
            break;
    }

    auto expr = parseExpression();
    expect(Token::Type::SEMICOLON, "Expected ';' after expression", true);
    auto node = makeTypedNode(ASTNode::Type::EXPRESSION_STATEMENT, 0);
    node->children.push_back(expr);
    return node;
}

std::shared_ptr<ASTNode> Parser::parseBlock(int depth) {
    expect(Token::Type::LBRACE, "Expected '{' at start of block", true);
    auto node = makeTypedNode(ASTNode::Type::BLOCK, 0);
    while (peek().type != Token::Type::RBRACE && peek().type != Token::Type::END_OF_FILE)
        node->children.push_back(parseStatement(depth+1));
    expect(Token::Type::RBRACE, "Expected '}' at end of block", true);
    return node;
}

std::shared_ptr<ASTNode> Parser::parseExpression() {
    return parseBinaryOp(parsePrimary());
}

std::shared_ptr<ASTNode> Parser::parsePrimary() {
    const Token &tok = peek();
    if (tok.type == Token::Type::NUMBER) {
        consume();
        auto node = makeTypedNode(ASTNode::Type::NUMBER, 1);
        node->strValue = tok.value;
        return node;
    }

    if (tok.type == Token::Type::LBRACKET) {
        return parseArrayLiteral();
    }

    if(tok.type == Token::Type::KEYWORD) {
        if(tok.value == "true" || tok.value == "false") {
            consume();
            auto node = makeTypedNode(ASTNode::Type::BOOL, 1);
            node->strValue = tok.value == "true" ? "1" : "0";
            return node;
        }
        throw std::runtime_error("Unexpected keyword: " + tok.value);
    }

    if (tok.type == Token::Type::SELF_REFERENCE) {
        consume();
        std::shared_ptr<ASTNode> node = makeTypedNode(ASTNode::Type::SELF_REFERENCE, 0);

        while (true) {
            if (match(Token::Type::LBRACKET)) {
                consume();
                auto indicesNode = makeTypedNode(ASTNode::Type::BLOCK, 0);
                while (true) {
                    auto start = parseExpression();
                    if (match(Token::Type::RANGE)) {
                        consume();
                        auto end = parseExpression();
                        auto rangeNode = makeTypedNode(ASTNode::Type::RANGE, 0);
                        rangeNode->children.push_back(start);
                        rangeNode->children.push_back(end);
                        indicesNode->children.push_back(rangeNode);
                    } else {
                        indicesNode->children.push_back(start);
                    }
                    if (match(Token::Type::COMMA)) {
                        consume();
                        continue;
                    }
                    break;
                }
                expect(Token::Type::RBRACKET, "Expected ']' after array index", true);

                if (match(Token::Type::EQUAL)) {
                    consume();
                    auto valueNode = (match(Token::Type::LBRACKET)) ? parseArrayLiteral() : parseExpression();
                    auto assignNode = makeTypedNode(ASTNode::Type::ARRAY_ASSIGN, 0);
                    assignNode->children.push_back(node);
                    assignNode->children.push_back(indicesNode);
                    assignNode->children.push_back(valueNode);
                    return assignNode;
                }

                auto accessNode = makeTypedNode(ASTNode::Type::ARRAY_ACCESS, 0);
                accessNode->children.push_back(node);
                accessNode->children.push_back(indicesNode);
                node = accessNode;
            } else {
                break;
            }
        }

        return node;
    }


    if (tok.type == Token::Type::STRING) {
        consume();
        auto node = makeTypedNode(ASTNode::Type::STRING, 1);
        node->strValue = tok.value;
        return node;
    }

    if (tok.type == Token::Type::IDENTIFIER) {
        consume();
        auto identNode = makeTypedNode(ASTNode::Type::IDENTIFIER, 1);
        identNode->strValue = tok.value;

        std::shared_ptr<ASTNode> node = identNode;

        while (true) {
            if (match(Token::Type::LBRACKET)) {
                consume();
                auto indicesNode = makeTypedNode(ASTNode::Type::BLOCK, 0);
                while (true) {
                    auto start = parseExpression();
                    if (match(Token::Type::RANGE)) {
                        consume();
                        auto end = parseExpression();
                        auto rangeNode = makeTypedNode(ASTNode::Type::RANGE, 0);
                        rangeNode->children.push_back(start);
                        rangeNode->children.push_back(end);
                        indicesNode->children.push_back(rangeNode);
                    } else {
                        indicesNode->children.push_back(start);
                    }
                    if (match(Token::Type::COMMA)) {
                        consume();
                        continue;
                    }
                    break;
                }
                expect(Token::Type::RBRACKET, "Expected ']' after array index", true);

                if (match(Token::Type::EQUAL)) {
                    consume();
                    auto valueNode = (match(Token::Type::LBRACKET)) ? parseArrayLiteral() : parseExpression();
                    auto assignNode = makeTypedNode(ASTNode::Type::ARRAY_ASSIGN, 0);
                    assignNode->children.push_back(node);
                    assignNode->children.push_back(indicesNode);
                    assignNode->children.push_back(valueNode);
                    return assignNode;
                }

                auto accessNode = makeTypedNode(ASTNode::Type::ARRAY_ACCESS, 0);
                accessNode->children.push_back(node);
                accessNode->children.push_back(indicesNode);
                node = accessNode;
            }
            else if (match(Token::Type::READ)) {
                consume();
                auto leftNode = node;

                while (true) {
                    auto rightNode = makeTypedNode(ASTNode::Type::IDENTIFIER, 1);
                    rightNode->strValue = expect(Token::Type::IDENTIFIER, "Expected identifier after '.'", true).value;

                    auto readNode = makeTypedNode(ASTNode::Type::READ, 0);
                    readNode->children.push_back(leftNode);
                    readNode->children.push_back(rightNode);
                    leftNode = readNode;

                    if (!match(Token::Type::READ)) break;
                    consume();
                }

                node = leftNode;

                if (match(Token::Type::EQUAL)) {
                    consume();
                    auto assignNode = makeTypedNode(ASTNode::Type::PRIMITIVE_ASSIGNMENT, 0);
                    assignNode->children.push_back(node);
                    assignNode->children.push_back(parseExpression());
                    return assignNode;
                }
            }
            else if (match(Token::Type::LPAREN)) {
                consume();
                auto callNode = makeTypedNode(ASTNode::Type::CALL, 0);
                callNode->children.push_back(node);

                while (peek().type != Token::Type::RPAREN) {
                    auto val1 = parseExpression();
                    if (match(Token::Type::COMMA)) consume();
                    if (match(Token::Type::RANGE)) {
                        consume();
                        auto val2 = parseExpression();
                        auto rangeNode = makeTypedNode(ASTNode::Type::RANGE, 0);
                        rangeNode->children.push_back(val1);
                        rangeNode->children.push_back(val2);
                        callNode->children.push_back(rangeNode);
                    } else {
                        callNode->children.push_back(val1);
                    }
                    if (peek().type == Token::Type::COMMA) consume();
                }
                expect(Token::Type::RPAREN, "Expected ')' after function arguments", true);
                node = callNode;
            }
            else {
                break; // no more chaining
            }
        }

        return node;
    }

    if (tok.type == Token::Type::LPAREN) {
        consume();
        auto node = parseExpression();
        expect(Token::Type::RPAREN, "Expected ')' after expression", true);
        return node;
    }

    if (tok.type == Token::Type::MINUS || tok.type == Token::Type::NOT || tok.type == Token::Type::BITWISE_NOT) {
        consume();
        auto node = makeTypedNode(ASTNode::Type::UNARY_OP, 0);
        node->binopValue = tok.binopValue;
        node->children.push_back(parsePrimary());
        return node;
    }

    error("Unexpected token in primary expression: " + tok.value);
    return nullptr;
}

std::shared_ptr<ASTNode> Parser::parseBinaryOp(std::shared_ptr<ASTNode> left, int minPrecedence) {
    while (true) {
        const int prec = getPrecedence(peek().type);
        if (prec < minPrecedence) break;
        const Token op = consume();
        auto right = parsePrimary();
        const int nextPrec = getPrecedence(peek().type);
        if (prec < nextPrec) right = parseBinaryOp(right, prec + 1);
        auto node = makeTypedNode(ASTNode::Type::BINARY_OP, 0);
        node->binopValue = op.binopValue;
        node->children.push_back(left);
        node->children.push_back(right);
        left = node;
    }
    return left;
}
