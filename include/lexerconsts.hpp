#ifndef LEXER_CONSTS_H
#define LEXER_CONSTS_H

#include <unordered_map>
#include "lexer.hpp"

const std::unordered_map<std::string, Token::Type> operators = {
    {"&&", Token::Type::AND}, {"||", Token::Type::OR},
    {"==", Token::Type::COMPARISON}, {"=", Token::Type::EQUAL},
    {"->", Token::Type::ARROW}, {"&", Token::Type::BITWISE_AND},
    {"|", Token::Type::BITWISE_OR}, {"^", Token::Type::BITWISE_XOR},
    {"~", Token::Type::BITWISE_NOT}, {"*", Token::Type::MULTIPLY},
    //{"-", Token::Type::MINUS}, {"+", Token::Type::PLUS},
    {"/", Token::Type::DIVIDE}, {"%", Token::Type::MODULUS},
    {"!", Token::Type::NOT}, {"..", Token::Type::RANGE},
    {"<", Token::Type::LESS}, {">", Token::Type::GREATER},
    {"<=", Token::Type::LESS_EQUAL}, {">=", Token::Type::GREATER_EQUAL},
    {";", Token::Type::SEMICOLON}, {",", Token::Type::COMMA},
    {"(", Token::Type::LPAREN}, {")", Token::Type::RPAREN},
    {"[", Token::Type::LBRACKET}, {"]", Token::Type::RBRACKET},
    {"{", Token::Type::LBRACE}, {"}", Token::Type::RBRACE},
    {"?", Token::Type::QUESTION_MARK}, {":", Token::Type::COLON},
    // {"@", Token::Type::SELF_REFERENCE}, 
    {".", Token::Type::READ}, {"...", Token::Type::SPREAD},
};

const std::unordered_map<std::string, Token::Type> keywords = {
    {"if", Token::Type::KEYWORD}, {"else", Token::Type::KEYWORD},
    {"while", Token::Type::KEYWORD}, {"return", Token::Type::KEYWORD},
    {"void", Token::Type::KEYWORD}, {"fin", Token::Type::KEYWORD},
    {"for", Token::Type::KEYWORD}, {"struct", Token::Type::KEYWORD},
    {"import", Token::Type::KEYWORD}, {"export", Token::Type::KEYWORD},
    {"false", Token::Type::KEYWORD}, {"true", Token::Type::KEYWORD},
    {"as", Token::Type::KEYWORD}, {"native", Token::Type::KEYWORD},
    {"link", Token::Type::KEYWORD},
};

const std::unordered_map<std::string, Primitive> primitives = {
    {"int", Primitive::INT},
    {"bool", Primitive::BOOL},
    {"string", Primitive::STRING},
};

const std::unordered_map<std::string, BinaryOp> binopMap = {
    {"+", PLUS}, {"-", MINUS}, {"*", MULTIPLY}, {"/", DIVIDE}, {"%", MODULUS},
    {"&", BITWISE_AND}, {"|", BITWISE_OR}, {"^", BITWISE_XOR}, {"~", BITWISE_NOT},
    {"!", NOT}, {"==", COMPARISON}, {"<", LESS}, {">", GREATER},
    {"<=", LESS_EQUAL}, {">=", GREATER_EQUAL},
};

#endif 