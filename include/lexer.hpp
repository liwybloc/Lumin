#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include <cstdint>

enum BinaryOp {
    PLUS,
    MINUS,
    MULTIPLY,
    DIVIDE,
    MODULUS,
    COMPARISON,
    LESS,
    GREATER,
    LESS_EQUAL,
    GREATER_EQUAL,
    NOT,
    BITWISE_NOT,
    BITWISE_AND,
    BITWISE_OR,
    BITWISE_XOR,
};

enum class Primitive {
    NONE,

    INT,
    BOOL,
    STRING,
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
        RANGE,

        READ,

        BITWISE_AND,
        BITWISE_OR,
        BITWISE_XOR,
        BITWISE_NOT,

        QUESTION_MARK,
        COLON,

        INCREMENT,
        DECREMENT,

        NUMBER,
        STRING,
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

private:
    std::string source;
    size_t current = 0;
    size_t lineIndex = 1;
    size_t colIndex = 1;

    char peek(size_t n = 0) const;
    char consume();
    void selfUpd(std::vector<Token> *tokens, const std::string &value, Token::Type type, unsigned long tokenLine, unsigned long tokenCol, int by);
    void pushSelfUpd(std::vector<Token> *tokens, const std::string &value, Token::Type type, unsigned long tokenLine, unsigned long tokenCol, int by);
    void simplitiveBinOp(std::vector<Token> *tokens, const std::string &value, Token::Type type, unsigned long tokenLine, unsigned long tokenCol);
};

#endif
