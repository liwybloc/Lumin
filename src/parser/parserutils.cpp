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

std::shared_ptr<ASTNode> makeNode(ASTNode::Type t, int valueType) {
    auto node = std::make_shared<ASTNode>();
    node->type = t;
    node->valueType = valueType;
    return node;
}