#ifndef LEXER_H
#define LEXER_H

#include <unordered_map>
#include <string>
#include <vector>
#include <cstdint>

enum BinaryOp {
    // special
    TERNARY,

    // many compatible ops
    PLUS,
    MULTIPLY,
    COMPARISON,
    NOT_EQUAL,

    STRING_END,

    // numeric arithmetic
    MINUS = STRING_END,
    DIVIDE,
    MODULUS,
    BITWISE_NOT,
    BITWISE_AND,
    BITWISE_OR,
    BITWISE_XOR,
    LESS,
    GREATER,
    LESS_EQUAL,
    GREATER_EQUAL,

    ARITH_END,

    // comparisons + logical ops
    NOT = ARITH_END,
    AND,
    OR,

    BOOL_END
};

enum class Primitive {
    NONE,

    INT,
    BOOL,
    STRING,
    CHAR,

    UNKNOWN,
};

struct Token {
    enum class Type {
        END_OF_FILE,

        SEMICOLON,
        COMMA,
        EQUAL,
        LBRACKET,
        RBRACKET,
        LBRACE,
        RBRACE,
        LPAREN,
        RPAREN,
        PLUS,
        MINUS,
        MULTIPLY,
        DIVIDE,
        MODULUS,
        COMPARISON,
        NOT,

        LESS,
        GREATER,
        LESS_EQUAL,
        GREATER_EQUAL,

        AND,
        OR,

        ARROW,
        SELF_REFERENCE,

        READ,
        RANGE,
        SPREAD,

        BITWISE_AND,
        BITWISE_OR,
        BITWISE_XOR,
        BITWISE_NOT,

        QMARK,
        COLON,

        INCREMENT,
        DECREMENT,

        NUMBER,
        STRING,
        CHAR,
        BOOL,
        IDENTIFIER,
        KEYWORD,
        PRIMITIVE,
    } type;
    std::string value;
    size_t lineIndex;
    size_t colIndex;

    BinaryOp binopValue;
    Primitive primitiveValue;
};

class Lexer {
public:
    Lexer(const std::string &source);
    std::vector<Token> tokenize();

    std::unordered_map<std::string, std::string> aliases;

private:
    std::string source;
    size_t current = 0;
    size_t lineIndex = 1;
    size_t colIndex = 1;

    char peek(size_t n = 0) const;
    char consume();
    [[noreturn]] void error(const std::string &message) const;
    char expect(const char expected, const std::string &message);
    void selfUpd(std::vector<Token> *tokens, const std::string &value, Token::Type type, unsigned long tokenLine, unsigned long tokenCol, int by);
    void pushSelfUpd(std::vector<Token> *tokens, const std::string &value, Token::Type type, unsigned long tokenLine, unsigned long tokenCol, int by);
    void simplitiveBinOp(std::vector<Token> *tokens, const std::string &value, Token::Type type, unsigned long tokenLine, unsigned long tokenCol);
    bool skipWhitespace();
    void applyHeader(const std::string &header);
};

#endif
