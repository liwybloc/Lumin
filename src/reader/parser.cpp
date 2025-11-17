#include "parser.hpp"
#include <stdexcept>
#include <tuple>
#include <sstream>

static std::shared_ptr<ASTNode> makeNode(ASTNode::Type t = ASTNode::Type::IDENTIFIER, int valueType = 67) {
    auto node = std::make_shared<ASTNode>();
    node->type = t;
    node->valueType = valueType;
    return node;
}

Parser::Parser(const std::vector<Token> &tokens) : tokens(tokens), current(0) {}

const Token &Parser::peek(size_t n) const {
    if (current + n >= tokens.size()) return tokens.back();
    return tokens[current + n];
}

const Token &Parser::consume() {
    if (current >= tokens.size()) return tokens.back();
    return tokens[current++];
}

bool Parser::match(Token::Type type, int offset) {
    return peek(offset).type == type;
}

Token Parser::expectMultiple(const std::vector<Token::Type> &types, const std::string &err) {
    for (const auto &t : types) {
        if (match(t)) return consume();
    }
    throw std::runtime_error(err);
}

Token Parser::expect(Token::Type type, const std::string &err, bool doConsume) {
    if (peek().type != type) {
        error(err);
    }
    return doConsume ? consume() : peek();
}

void Parser::error(const std::string &msg) const {
    throw std::runtime_error(msg + " at " + std::to_string(peek().lineIndex) + ":" + std::to_string(peek().colIndex));
}

std::shared_ptr<ASTNode> Parser::parse() {
    return parseProgram();
}

std::shared_ptr<ASTNode> Parser::parseProgram() {
    auto node = makeNode(ASTNode::Type::PROGRAM);
    while (peek().type != Token::Type::END_OF_FILE)
        node->children.push_back(parseStatement());
    return node;
}

std::shared_ptr<ASTNode> Parser::parseArrayLiteral() {
    expect(Token::Type::LBRACKET, "Expected '[' after array declaration", true);
    auto node = makeNode(ASTNode::Type::ARRAY_LITERAL);
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
    auto sad = makeNode(ASTNode::Type::SIZED_ARRAY_DECLARE);
    sad->valueType = 1;
    if (isPrimitive) sad->primitiveValue = typeToken.primitiveValue;
    sad->strValue = typeToken.value;
    sad->children.push_back(sizeNode);
    return sad;
}

std::shared_ptr<ASTNode> Parser::buildTypeNodeFromToken(const Token &typeToken) {
    auto typeNode = makeNode(ASTNode::Type::IDENTIFIER);
    typeNode->strValue = typeToken.value;
    typeNode->valueType = 1;
    return typeNode;
}

std::shared_ptr<ASTNode> Parser::parseDeclarationWithTypeAndName(const Token &typeToken, const Token &nameToken, bool isPrimitive) {
    bool isArray = false;
    std::shared_ptr<ASTNode> arraySize = parseOptionalArraySize(isArray);

    auto node = makeNode(isPrimitive ? ASTNode::Type::PRIMITIVE_ASSIGNMENT : ASTNode::Type::STRUCT_ASSIGNMENT);
    node->valueType = 1;
    node->strValue = nameToken.value;

    if (!isPrimitive) {
        auto typeNode = buildTypeNodeFromToken(typeToken);
        node->children.push_back(typeNode);
    } else {
        node->primitiveValue = typeToken.primitiveValue;
    }

    if (match(Token::Type::EQUAL)) {
        if (arraySize != nullptr) {
            error("Array data cannot be initialized if an empty sized set is specified");
        }
        consume();

        auto exprNode = parseExpression();

        if (!isPrimitive) {
            if (exprNode->type != ASTNode::Type::IDENTIFIER &&
                exprNode->type != ASTNode::Type::CALL) {
                error("Structs can only be initialized from a variable or function returning a struct");
            }
            node->children.push_back(exprNode);
        } else {
            if (isArray) {
                node->children.push_back(parseArrayLiteral());
            } else {
                node->children.push_back(exprNode);
            }
        }
    } else if (arraySize != nullptr) {
        node->children.push_back(buildSizedArrayDeclareNode(typeToken, arraySize, isPrimitive));
    }

    expect(Token::Type::SEMICOLON, "Expected ';' after assignment", true);
    return node;
}

std::shared_ptr<ASTNode> Parser::parseStatement() {
    const Token &tok = peek();

    if (match(Token::Type::LBRACE)) return parseBlock();

    if (match(Token::Type::IDENTIFIER) && match(Token::Type::IDENTIFIER, 1)) {
        const Token typeToken = consume();
        const Token nameToken = consume();
        auto node = parseDeclarationWithTypeAndName(typeToken, nameToken, false);
        return node;
    }

    if (tok.type == Token::Type::PRIMITIVE) {
        const Token typeToken = consume();
        const Token nameToken = expect(Token::Type::IDENTIFIER, "Expected identifier after type", true);
        auto node = parseDeclarationWithTypeAndName(typeToken, nameToken, true);
        return node;
    }

    if (tok.type == Token::Type::KEYWORD) {
        const std::string kw = consume().value;
        auto node = makeNode();

        if (kw == "return") {
            node->type = ASTNode::Type::RETURN_STATEMENT;
            node->children.push_back(parseExpression());
            expect(Token::Type::SEMICOLON, "Expected ';' after return statement", true);
            return node;
        }

        if (kw == "if") {
            node->type = ASTNode::Type::IF_STATEMENT;
            expect(Token::Type::LPAREN, "Expected '(' after if", true);
            node->children.push_back(parseExpression());
            expect(Token::Type::RPAREN, "Expected ')' after if condition", true);
            node->children.push_back(parseStatement());
            if (match(Token::Type::KEYWORD) && peek().value == "else") {
                consume();
                auto elseNode = makeNode(ASTNode::Type::ELSE_STATEMENT);
                elseNode->children.push_back(parseStatement());
                node->children.push_back(elseNode);
            }
            return node;
        }

        if (kw == "while") {
            node->type = ASTNode::Type::WHILE_STATEMENT;
            expect(Token::Type::LPAREN, "Expected '(' after while", true);
            node->children.push_back(parseExpression());
            expect(Token::Type::RPAREN, "Expected ')' after while condition", true);
            node->children.push_back(parseStatement());
            return node;
        }

        if (kw == "fin") {
            node->type = ASTNode::Type::FUNCTION;
            node->valueType = 1;
            node->strValue = expect(Token::Type::IDENTIFIER, "Expected identifier after 'fin'", true).value;
            expect(Token::Type::LPAREN, "Expected '(' after function name", true);
            while (match(Token::Type::IDENTIFIER)) {
                auto paramNode = makeNode(ASTNode::Type::IDENTIFIER);
                paramNode->strValue = consume().value;
                node->children.push_back(paramNode);
                if (match(Token::Type::COMMA)) consume();
            }
            expect(Token::Type::RPAREN, "Expected ')' after function parameters", true);
            expect(Token::Type::ARROW, "Expected '->' after function parameters", true);
            const Token& t = consume();
            if (!(t.type == Token::Type::PRIMITIVE || t.type == Token::Type::IDENTIFIER))
                error("Expected type after arrow");
            node->retType = t.value;
            if (t.type == Token::Type::PRIMITIVE)
                node->primitiveValue = t.primitiveValue;
            if (match(Token::Type::LBRACE)) node->children.push_back(parseBlock());
            else node->children.push_back(parseStatement());
            return node;
        }

        if (kw == "for") {
            node->type = ASTNode::Type::FOR_STATEMENT;
            expect(Token::Type::LPAREN, "Expected '(' after for", true);
            node->children.push_back(parseStatement());
            node->children.push_back(parseExpression());
            expect(Token::Type::SEMICOLON, "Expected ';' after for loop condition", true);
            node->children.push_back(parseExpression());
            expect(Token::Type::RPAREN, "Expected ')' after for loop increment", true);
            node->children.push_back(parseStatement());
            return node;
        }

        if (kw == "struct") {
            node->type = ASTNode::Type::STRUCT_DECLARE;
            node->valueType = 1;
            node->strValue = expect(Token::Type::IDENTIFIER, "Expected struct name following struct declaration", true).value;
            expect(Token::Type::LBRACE, "Expected '{' after struct declaration", true);
            while (peek().type != Token::Type::RBRACE && peek().type != Token::Type::END_OF_FILE) {
                if (match(Token::Type::PRIMITIVE) || match(Token::Type::IDENTIFIER)) {
                    node->children.push_back(parseStatement());
                } else {
                    break;
                }
            }
            expect(Token::Type::RBRACE, "Expected '}' after struct declaration", true);
            expect(Token::Type::SEMICOLON, "Expected ';' after struct declaration", true);
            return node;
        }

        if(kw == "import") {
            node->type = ASTNode::Type::IMPORT;
            node->valueType = 1;
            node->strValue = expect(Token::Type::STRING, "Expected import string following import declaration", true).value;
            expect(Token::Type::SEMICOLON, "Expected ';' after import statement", true);
            return node;
        }

        throw std::runtime_error("Unexpected keyword: " + kw);
    }

    auto exprNode = parseExpression();
    expect(Token::Type::SEMICOLON, "Expected ';' after expression", true);
    auto node = makeNode(ASTNode::Type::EXPRESSION_STATEMENT);
    node->children.push_back(exprNode);
    return node;
}

std::shared_ptr<ASTNode> Parser::parseBlock() {
    expect(Token::Type::LBRACE, "Expected '{' at start of block", true);
    auto node = makeNode(ASTNode::Type::BLOCK);
    while (peek().type != Token::Type::RBRACE && peek().type != Token::Type::END_OF_FILE)
        node->children.push_back(parseStatement());
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
        auto node = makeNode(ASTNode::Type::NUMBER);
        node->strValue = tok.value;
        node->valueType = 1;
        return node;
    }

    if (tok.type == Token::Type::SELF_REFERENCE) {
        consume();
        return makeNode(ASTNode::Type::SELF_REFERENCE);
    }

    if (tok.type == Token::Type::STRING) {
        consume();
        auto node = makeNode(ASTNode::Type::STRING);
        node->strValue = tok.value;
        node->valueType = 1;
        return node;
    }

    if (tok.type == Token::Type::IDENTIFIER) {
        consume();
        auto identNode = makeNode(ASTNode::Type::IDENTIFIER);
        identNode->strValue = tok.value;
        identNode->valueType = 1;

        if (match(Token::Type::LPAREN)) {
            consume();
            auto callNode = makeNode(ASTNode::Type::CALL);
            callNode->children.push_back(identNode);

            auto argsNode = makeNode(ASTNode::Type::BLOCK);
            while (peek().type != Token::Type::RPAREN) {
                std::shared_ptr<ASTNode> val1 = parseExpression();
                if (match(Token::Type::COMMA)) consume();
                if (match(Token::Type::RANGE)) {
                    consume();
                    std::shared_ptr<ASTNode> val2 = parseExpression();
                    auto rangeNode = makeNode(ASTNode::Type::RANGE);
                    rangeNode->children.push_back(val1);
                    rangeNode->children.push_back(val2);
                    argsNode->children.push_back(rangeNode);
                } else {
                    argsNode->children.push_back(val1);
                }
                if (peek().type == Token::Type::COMMA) consume();
            }
            expect(Token::Type::RPAREN, "Expected ')' after function arguments", true);
            callNode->children.push_back(argsNode);
            return callNode;
        }

        if (match(Token::Type::LBRACE)) {
            consume();
            auto arrayNode = makeNode(ASTNode::Type::NDARRAY_ASSIGN);
            arrayNode->valueType = 1;
            arrayNode->strValue = tok.value;
            while (!match(Token::Type::RBRACE)) {
                arrayNode->children.push_back(parseExpression());
                if (match(Token::Type::COMMA)) consume();
            }
            expect(Token::Type::RBRACE, "Expected '}' after ndarray element data", true);
            if (match(Token::Type::EQUAL)) {
                consume();
                auto valueNode = match(Token::Type::LBRACKET) ? parseArrayLiteral() : parseExpression();
                arrayNode->children.push_back(valueNode);
            }
            return arrayNode;
        }

        if (match(Token::Type::LBRACKET)) {
            consume();
            auto indicesNode = makeNode(ASTNode::Type::BLOCK);
            while (true) {
                auto start = parseExpression();
                if (match(Token::Type::RANGE)) {
                    consume();
                    auto end = parseExpression();
                    auto rangeNode = makeNode(ASTNode::Type::RANGE);
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
                auto assignNode = makeNode(ASTNode::Type::ARRAY_ASSIGN);
                assignNode->children.push_back(identNode);
                assignNode->children.push_back(indicesNode);
                assignNode->children.push_back(valueNode);
                return assignNode;
            }

            auto accessNode = makeNode(ASTNode::Type::ARRAY_ACCESS);
            accessNode->children.push_back(identNode);
            accessNode->children.push_back(indicesNode);
            identNode = accessNode;
        }

        if (match(Token::Type::READ)) {
            consume();
            auto leftNode = identNode;

            while (true) {
                auto rightNode = makeNode(ASTNode::Type::IDENTIFIER);
                rightNode->valueType = 1;
                rightNode->strValue = expect(Token::Type::IDENTIFIER, "Expected identifier after '.'", true).value;

                auto readNode = makeNode(ASTNode::Type::READ);
                readNode->children.push_back(leftNode);
                readNode->children.push_back(rightNode);
                leftNode = readNode;

                if (!match(Token::Type::READ)) break;
                consume();
            }

            if (match(Token::Type::EQUAL)) {
                consume();
                auto assignNode = makeNode(ASTNode::Type::PRIMITIVE_ASSIGNMENT);
                assignNode->children.push_back(leftNode);
                assignNode->children.push_back(parseExpression());
                return assignNode;
            }

            identNode = leftNode;
        }

        if (match(Token::Type::EQUAL)) {
            consume();
            auto assignNode = makeNode(ASTNode::Type::PRIMITIVE_ASSIGNMENT);
            assignNode->strValue = tok.value;
            assignNode->children.push_back(parseExpression());
            return assignNode;
        }

        return identNode;
    }

    if (tok.type == Token::Type::LPAREN) {
        consume();
        auto node = parseExpression();
        expect(Token::Type::RPAREN, "Expected ')' after expression", true);
        return node;
    }

    if (tok.type == Token::Type::MINUS || tok.type == Token::Type::NOT || tok.type == Token::Type::BITWISE_NOT) {
        consume();
        auto node = makeNode(ASTNode::Type::UNARY_OP);
        node->binopValue = tok.binopValue;
        node->valueType = 0;
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
        auto node = makeNode(ASTNode::Type::BINARY_OP);
        node->binopValue = op.binopValue;
        node->valueType = 0;
        node->children.push_back(left);
        node->children.push_back(right);
        left = node;
    }
    return left;
}

int Parser::getPrecedence(Token::Type type) const {
    switch(type) {
        case Token::Type::MULTIPLY:
        case Token::Type::DIVIDE:
        case Token::Type::MODULUS:
            return 3;
        case Token::Type::PLUS:
        case Token::Type::MINUS:
            return 2;
        case Token::Type::EQUAL:
        case Token::Type::COMPARISON:
        case Token::Type::LESS:
        case Token::Type::GREATER:
        case Token::Type::LESS_EQUAL:
        case Token::Type::GREATER_EQUAL:
            return 1;
        case Token::Type::AND:
        case Token::Type::OR:
            return 0;
        default:
            return -1;
    }
}

std::string typeToString(ASTNode::Type type) {
    switch(type) {
        case ASTNode::Type::PROGRAM: return "PROGRAM";
        case ASTNode::Type::NUMBER: return "NUMBER";
        case ASTNode::Type::RANGE: return "RANGE";
        case ASTNode::Type::STRING: return "STRING";
        case ASTNode::Type::BOOL: return "BOOL";
        case ASTNode::Type::IDENTIFIER: return "IDENTIFIER";
        case ASTNode::Type::SELF_REFERENCE: return "SELF_REFERENCE";
        case ASTNode::Type::BINARY_OP: return "BINARY_OP";
        case ASTNode::Type::UNARY_OP: return "UNARY_OP";
        case ASTNode::Type::PRIMITIVE_ASSIGNMENT: return "PRIMITIVE_ASSIGNMENT";
        case ASTNode::Type::STRUCT_ASSIGNMENT: return "STRUCT_ASSIGNMENT";
        case ASTNode::Type::SIZED_ARRAY_DECLARE: return "SIZED_ARRAY_DECLARE";
        case ASTNode::Type::ELSE_STATEMENT: return "ELSE_STATEMENT";
        case ASTNode::Type::EXPRESSION_STATEMENT: return "EXPRESSION_STATEMENT";
        case ASTNode::Type::BLOCK: return "BLOCK";
        case ASTNode::Type::CALL: return "CALL";
        case ASTNode::Type::IF_STATEMENT: return "IF_STATEMENT";
        case ASTNode::Type::WHILE_STATEMENT: return "WHILE_STATEMENT";
        case ASTNode::Type::RETURN_STATEMENT: return "RETURN_STATEMENT";
        case ASTNode::Type::FUNCTION: return "FUNCTION";
        case ASTNode::Type::FOR_STATEMENT: return "FOR_STATEMENT";
        case ASTNode::Type::ARRAY_LITERAL: return "ARRAY_LITERAL";
        case ASTNode::Type::ARRAY_ACCESS: return "ARRAY_ACCESS";
        case ASTNode::Type::ARRAY_ASSIGN: return "ARRAY_ASSIGN";
        case ASTNode::Type::READ: return "READ";
        case ASTNode::Type::NDARRAY_ASSIGN: return "NDARRAY_ASSIGN";
        case ASTNode::Type::STRUCT_DECLARE: return "STRUCT_DECLARE";
        default: return "UNKNOWN";
    }
}

std::string astToString(const std::shared_ptr<ASTNode> &node, int indent) {
    std::stringstream ss;
    std::string ind(indent * 2, ' ');
    ss << ind << typeToString(node->type);
    if (!node->strValue.empty()) {
        ss << "{\"" << node->strValue << "\"}";
    }
    if (node->type == ASTNode::Type::NUMBER) {
        ss << "{" << static_cast<int>(node->primitiveValue) << "}";
    }
    if (node->type == ASTNode::Type::BINARY_OP || node->type == ASTNode::Type::UNARY_OP) {
        ss << "{" << static_cast<int>(node->binopValue) << "}";
    }
    if (!node->children.empty()) {
        ss << ".[\n";
        for (size_t i = 0; i < node->children.size(); ++i) {
            ss << astToString(node->children[i], indent + 1);
            if (i + 1 < node->children.size()) ss << ",\n";
        }
        ss << "\n" << ind << "]";
    }
    return ss.str();
}