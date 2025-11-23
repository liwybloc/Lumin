#include "parser.hpp"
#include "parserutils.hpp"

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

std::unordered_map<std::string, KwHandler> Parser::initKwMap() {
    return {
        {
            "return",
            [](Parser* p, int depth) {
                auto node = makeNode(ASTNode::Type::RETURN_STATEMENT);
                node->children.push_back(p->parseExpression());
                p->expect(Token::Type::SEMICOLON, "Expected ';' after return statement", true);
                return node;
            }
        },
        {
            "if",
            [](Parser* p, int depth) {
                auto node = makeNode(ASTNode::Type::IF_STATEMENT);
                p->expect(Token::Type::LPAREN, "Expected '(' after if", true);
                node->children.push_back(p->parseExpression());
                p->expect(Token::Type::RPAREN, "Expected ')' after if condition", true);
                node->children.push_back(p->parseStatement(depth + 1));

                if (p->match(Token::Type::KEYWORD) && p->peek().value == "else") {
                    p->consume();
                    auto elseNode = makeNode(ASTNode::Type::ELSE_STATEMENT);
                    elseNode->children.push_back(p->parseStatement(depth + 1));
                    node->children.push_back(elseNode);
                }
                return node;
            }
        },
        {
            "while",
            [](Parser* p, int depth) {
                auto node = makeNode(ASTNode::Type::WHILE_STATEMENT);
                p->expect(Token::Type::LPAREN, "Expected '(' after while", true);
                node->children.push_back(p->parseExpression());
                p->expect(Token::Type::RPAREN, "Expected ')' after while condition", true);
                node->children.push_back(p->parseStatement(depth + 1));
                return node;
            }
        },
        {
            "native",
            [](Parser* p, int depth) {
                if(!p->match(Token::Type::KEYWORD) || p->peek().value != "fin") {
                    p->error("Expected 'fin' keyword after 'native'");
                }
                auto func = p->parseStatement(depth);

                if(func->children.back()->children.size() != 0) {
                    p->error("Native functions cannot have a body");
                }

                auto nativeNode = makeNode(ASTNode::Type::NATIVE_STATEMENT);
                nativeNode->children.push_back(func);
                return nativeNode;
            }
        },
        {
            "fin",
            [](Parser* p, int depth) {
                auto node = makeNode(ASTNode::Type::FUNCTION);
                node->valueType = 1;

                Token first = p->peek();
                Token second = p->peek(1);

                bool alt = (first.type == Token::Type::PRIMITIVE || first.type == Token::Type::IDENTIFIER)
                        && second.type == Token::Type::IDENTIFIER;

                if (alt) {
                    Token t = p->consume();
                    Token name = p->consume();
                    node->retType = t.value;
                    if (t.type == Token::Type::PRIMITIVE) node->primitiveValue = t.primitiveValue;
                    node->strValue = name.value;
                } else {
                    node->strValue = p->expect(Token::Type::IDENTIFIER, "Expected identifier after 'fin'", true).value;
                }

                p->expect(Token::Type::LPAREN, "Expected '(' after function name", true);

                while (p->match(Token::Type::IDENTIFIER) || p->match(Token::Type::PRIMITIVE)) {
                    auto param = makeNode(ASTNode::Type::STRING);
                    std::shared_ptr<ASTNode> typeNode = makeTypedNode(ASTNode::Type::STRING, 1);

                    if (p->match(Token::Type::PRIMITIVE)) {
                        typeNode->primitiveValue = p->consume().primitiveValue;
                    } else {
                        typeNode->strValue = p->consume().value;
                    }
                    param->children.push_back(typeNode);

                    bool spread = false;
                    if(p->match(Token::Type::SPREAD)) {
                        p->consume();
                        // signifier
                        param->children.push_back(makeNode(ASTNode::Type::ARRAY_ASSIGN));
                        spread = true;
                    }

                    param->strValue = p->expect(Token::Type::IDENTIFIER, "Expected identifier after parameter", true).value;

                    node->children.push_back(param);

                    if(spread && !p->match(Token::Type::RPAREN))
                        p->error("Spread argument must be the last parameter");
                    else if (p->match(Token::Type::COMMA))
                        p->consume();
                }
                p->expect(Token::Type::RPAREN, "Expected ')' after function parameters", true);

                if (!alt) {
                    std::string value;
                    Primitive primitive = Primitive::NONE;
                    if(p->match(Token::Type::ARROW)) {
                        p->consume();
                        Token t = p->consume();
                        if (!(t.type == Token::Type::PRIMITIVE || t.type == Token::Type::IDENTIFIER))
                            p->error("Expected type after arrow");
                        value = t.value;
                        primitive = t.primitiveValue;
                    } else {
                        value = "nil";
                    }

                    node->retType = value;
                    if (primitive != Primitive::NONE) node->primitiveValue = primitive;
                }

                if(p->match(Token::Type::SEMICOLON)) {
                    p->consume();
                    node->children.push_back(makeNode(ASTNode::Type::BLOCK));
                    return node;
                }

                if (p->match(Token::Type::LBRACE))
                    node->children.push_back(p->parseBlock(depth + 1));
                else
                    node->children.push_back(p->parseStatement(depth + 1));

                return node;
            }
        },
        {
            "for",
            [](Parser* p, int depth) {
                auto node = makeNode(ASTNode::Type::FOR_STATEMENT);
                p->expect(Token::Type::LPAREN, "Expected '(' after for", true);

                // enhanced for
                if((p->match(Token::Type::IDENTIFIER) || p->match(Token::Type::PRIMITIVE))
                     && p->match(Token::Type::IDENTIFIER, 1)
                     && p->match(Token::Type::COLON, 2)) {
                    node->children.push_back(p->parseStatement(depth + 1, true));
                    p->consume(); // colon
                    auto name = p->expect(Token::Type::IDENTIFIER, "Expected identifier after enhanced for", true);
                    auto array = makeTypedNode(ASTNode::Type::IDENTIFIER, 0);
                    array->strValue = name.value;
                    node->children.push_back(array);
                    node->strValue = "1";
                } else {
                    node->children.push_back(p->parseStatement(depth + 1));
                    node->children.push_back(p->parseExpression());
                    p->expect(Token::Type::SEMICOLON, "Expected ';' after for loop condition", true);
                    node->children.push_back(p->parseExpression());
                    node->strValue = "0";
                }
                p->expect(Token::Type::RPAREN, "Expected ')' after for loop innards", true);
                node->children.push_back(p->parseStatement(depth + 1));
                return node;
            }
        },
        {
            "struct",
            [](Parser* p, int depth) {
                auto node = makeNode(ASTNode::Type::STRUCT_DECLARE);
                node->valueType = 1;
                node->strValue = p->expect(Token::Type::IDENTIFIER, "Expected struct name", true).value;

                p->expect(Token::Type::LBRACE, "Expected '{' after struct declaration", true);
                while (p->peek().type != Token::Type::RBRACE && p->peek().type != Token::Type::END_OF_FILE) {
                    if (p->match(Token::Type::PRIMITIVE) || p->match(Token::Type::IDENTIFIER))
                        node->children.push_back(p->parseStatement(depth + 1));
                    else
                        break;
                }
                p->expect(Token::Type::RBRACE, "Expected '}' after struct declaration", true);
                p->expect(Token::Type::SEMICOLON, "Expected ';' after struct declaration", true);
                return node;
            }
        },
        {
            "import",
            [](Parser* p, int depth) {
                if (depth != 0)
                    p->error("Import statements are only allowed at top-level");

                auto node = makeNode(ASTNode::Type::STRING);
                node->valueType = 1;
                node->strValue = p->expect(Token::Type::STRING, "Expected import string", true).value;

                if(node->strValue.ends_with(".lum")) {
                    std::string v = p->expect(Token::Type::KEYWORD, "Expected 'as' after import statement", true).value;
                    if(v != "as") p->error("Expected 'as' after import statement");

                    std::string alias = p->expect(Token::Type::IDENTIFIER, "Expected namespace identifier after 'as' in import statement", true).value;
                    auto aliasNode = makeTypedNode(ASTNode::Type::IDENTIFIER, 1);
                    aliasNode->strValue = alias;
                    node->children.push_back(aliasNode);
                }

                p->expect(Token::Type::SEMICOLON, "Expected ';' after import statement", true);
                p->importBlock->children.push_back(node);
                return nullptr;
            }
        },
        {
            "export",
            [](Parser* p, int depth) {
                if (depth!= 0)
                    p->error("Export statements are only allowed at top-level");

                if(!p->match(Token::Type::IDENTIFIER) && (!p->match(Token::Type::KEYWORD) || p->peek().value != "fin") && !p->match(Token::Type::PRIMITIVE)) {
                    p->error("Expected function, type, or primitive after export keyword");
                }
                
                auto node = p->parseStatement(0);

                if(node->type == ASTNode::Type::IDENTIFIER) {
                    p->error("Expected function, type, or primitive after export keyword");
                }
                
                auto dataNode = makeNode(ASTNode::Type::STRING);
                dataNode->valueType = 1;
                dataNode->strValue = node->strValue;
                p->exportBlock->children.push_back(dataNode);

                return node;
            }
        },
        {
            "true",
            [](Parser* p, int depth) {
                auto node = makeTypedNode(ASTNode::Type::BOOL, 1);
                node->strValue = "1";
                return node;
            }
        },
        {
            "false",
            [](Parser* p, int depth) {
                auto node = makeTypedNode(ASTNode::Type::BOOL, 1);
                node->strValue = "0";
                return node;
            }
        },
        {
            "link",
            [](Parser* p, int depth) {
                if(depth != 0)
                    throw std::runtime_error("Cannot link to dll outside of top-level");
                auto node = makeTypedNode(ASTNode::Type::STRING, 1);
                return node;
            }
        }
    };
}

std::string typeToString(Token::Type type) {
    switch (type) {
        case Token::Type::END_OF_FILE: return "END_OF_FILE";
        case Token::Type::SEMICOLON: return "SEMICOLON";
        case Token::Type::COMMA: return "COMMA";
        case Token::Type::EQUAL: return "EQUAL";
        case Token::Type::LBRACKET: return "LBRACKET";
        case Token::Type::RBRACKET: return "RBRACKET";
        case Token::Type::LBRACE: return "LBRACE";
        case Token::Type::RBRACE: return "RBRACE";
        case Token::Type::LPAREN: return "LPAREN";
        case Token::Type::RPAREN: return "RPAREN";
        case Token::Type::PLUS: return "PLUS";
        case Token::Type::MINUS: return "MINUS";
        case Token::Type::MULTIPLY: return "MULTIPLY";
        case Token::Type::DIVIDE: return "DIVIDE";
        case Token::Type::MODULUS: return "MODULUS";
        case Token::Type::COMPARISON: return "COMPARISON";
        case Token::Type::NOT: return "NOT";
        case Token::Type::LESS: return "LESS";
        case Token::Type::GREATER: return "GREATER";
        case Token::Type::LESS_EQUAL: return "LESS_EQUAL";
        case Token::Type::GREATER_EQUAL: return "GREATER_EQUAL";
        case Token::Type::AND: return "AND";
        case Token::Type::OR: return "OR";
        case Token::Type::ARROW: return "ARROW";
        case Token::Type::SELF_REFERENCE: return "SELF_REFERENCE";
        case Token::Type::RANGE: return "RANGE";
        case Token::Type::READ: return "READ";
        case Token::Type::BITWISE_AND: return "BITWISE_AND";
        case Token::Type::BITWISE_OR: return "BITWISE_OR";
        case Token::Type::BITWISE_XOR: return "BITWISE_XOR";
        case Token::Type::BITWISE_NOT: return "BITWISE_NOT";
        case Token::Type::QUESTION_MARK: return "QUESTION_MARK";
        case Token::Type::COLON: return "COLON";
        case Token::Type::INCREMENT: return "INCREMENT";
        case Token::Type::DECREMENT: return "DECREMENT";
        case Token::Type::NUMBER: return "NUMBER";
        case Token::Type::STRING: return "STRING";
        case Token::Type::BOOL: return "BOOL";
        case Token::Type::IDENTIFIER: return "IDENTIFIER";
        case Token::Type::KEYWORD: return "KEYWORD";
        case Token::Type::PRIMITIVE: return "PRIMITIVE";
        default: return "UNKNOWN";
    }
}

std::string astTypeToString(ASTNode::Type type) {
    switch(type) {
        case ASTNode::Type::PROGRAM: return "PROGRAM";
        case ASTNode::Type::PRAGMA: return "PRAGMA";
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
        case ASTNode::Type::IMPORT_BLOCK: return "IMPORT_BLOCK";
        default: return "UNKNOWN";
    }
}

std::string astToString(const std::shared_ptr<ASTNode> &node, int indent) {
    std::stringstream ss;
    std::string ind(indent * 2, ' ');
    ss << ind << astTypeToString(node->type);
    if (!node->strValue.empty()) {
        ss << "{\"" << node->strValue << "\"}";
    }
    if (node->primitiveValue != Primitive::NONE) {
        ss << "{" << static_cast<int>(node->primitiveValue) << "}";
    }
    if (node->type == ASTNode::Type::BINARY_OP || node->type == ASTNode::Type::UNARY_OP) {
        ss << "{" << static_cast<int>(node->binopValue) << "}";
    }
    if (node->type == ASTNode::Type::FUNCTION) {
        ss << "{" << node->retType << "}";
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

std::shared_ptr<ASTNode> makeTypedNode(ASTNode::Type t, int valueType) {
    auto node = std::make_shared<ASTNode>();
    node->type = t;
    node->valueType = valueType;
    return node;
}
// because of intellisense
std::shared_ptr<ASTNode> makeNode(ASTNode::Type t) {
    return makeTypedNode(t, 67);
}