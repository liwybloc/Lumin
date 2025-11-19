#include "lexer.hpp"
#include "lexerconsts.hpp"
#include <unordered_map>
#include <stdexcept>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <numeric>
#include <regex>

Lexer::Lexer(const std::string &source) : source(source), current(0), lineIndex(1), colIndex(1) {}

char Lexer::peek(size_t n) const {
    if (current + n >= source.size()) return '\0';
    return source[current + n];
}

char Lexer::consume() {
    if (current >= source.size()) return '\0';
    char c = source[current++];
    if (c == '\n') {
        lineIndex++;
        colIndex = 1;
    } else {
        colIndex++;
    }
    return c;
}

void Lexer::selfUpd(std::vector<Token>* tokens, const std::string &value, Token::Type type, unsigned long tokenLine, unsigned long tokenCol, int by) {
    tokens->push_back(Token{Token::Type::SELF_REFERENCE, "@", tokenLine, tokenCol});
    tokens->push_back(Token{type, value, tokenLine, tokenCol});
    tokens->back().binopValue = binopMap.at(value);
    if(by != 0) tokens->push_back(Token{Token::Type::NUMBER, std::to_string(by), tokenLine, tokenCol});
}

void Lexer::pushSelfUpd(std::vector<Token>* tokens, const std::string &value, Token::Type type, unsigned long tokenLine, unsigned long tokenCol, int by) {
    tokens->push_back(Token{Token::Type::EQUAL, "=", tokenLine, tokenCol});
    selfUpd(tokens, value, type, tokenLine, tokenCol, by);
}

void Lexer::simplitiveBinOp(std::vector<Token>* tokens, const std::string &value, Token::Type type, unsigned long tokenLine, unsigned long tokenCol) {
    consume();
    if(peek() == *value.c_str()) {
        consume();
        pushSelfUpd(tokens, value, type, tokenLine, tokenCol, 1);
        return;
    }
    if(peek() == '=') {
        consume();
        pushSelfUpd(tokens, value, type, tokenLine, tokenCol, 0);
        return;
    }
    Token token{type, value, tokenLine, tokenCol};
    token.binopValue = binopMap.at(value);
    tokens->push_back(token);
}

bool Lexer::skipWhitespace() {
    if (std::isspace(static_cast<unsigned char>(peek()))) {
        consume();
        return true;
    }
    return false;
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}
std::string shift(std::vector<std::string> vec) {
    if (vec.empty()) return "";
    std::string first = vec.front();
    vec.erase(vec.begin());
    return first;
}

std::string join(const std::vector<std::string>& vec, const std::string& delimiter) {
    std::string result = std::accumulate(
        std::next(vec.begin()), vec.end(), vec[0],
        [delimiter](const std::string& a, const std::string& b) { return a + delimiter + b; }
    );
    return result;
}

void Lexer::applyHeader(const std::string &header) {
    const std::vector<std::string> args = split(header, ' ');
    if(args.empty()) throw std::runtime_error("Empty header");

    const std::string cmd = shift(args);
    
    if(cmd == "alias") {
        static const std::regex aliasPattern(R"(alias\s+\"([^\"]+)\"\s+as\s+\"([^\"]+)\")");

        std::smatch match;
        const std::string rest = join(args, " ");
        if(!std::regex_match(rest, match, aliasPattern)) {
            throw std::runtime_error(
                "Expected alias in format 'alias \"search\" as \"replace\"'"
            );
        }

        std::string from = match[1].str();
        std::string to = match[2].str();

        aliases[from] = to;
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    std::vector<std::pair<std::string, Token::Type>> sortedOps(operators.begin(), operators.end());
    std::sort(sortedOps.begin(), sortedOps.end(), [](const auto &a, const auto &b) {
        return a.first.size() > b.first.size();
    });

    while(peek()) {
        if(skipWhitespace()) continue;

        if(peek() == '#') {
            std::stringstream ss;
            consume();
            char c;
            while ((c = peek()) && c != '\n') {
                ss << consume();
            }

            std::string header = ss.str();
            applyHeader(header);

            continue;
        }

        break;
    }

    for (const auto& [from, to] : aliases) {
        std::regex re(R"((^|[^"])\b)" + from + R"(\b($|[^"]))");
        source = std::regex_replace(source, re, "$1" + to + "$2");
    }

    while (peek()) {
        if(skipWhitespace()) continue;

        if(peek() == '/') {
            if(peek(1) == '/') {
                while (peek() && peek() != '\n') consume();
                continue;
            } else if(peek(1) == '*') {
                while (peek() && peek(1) && peek() != '*' && peek(1) != '/') consume();
                continue;
            }
        }

        size_t tokenLine = lineIndex;
        size_t tokenCol = colIndex;

        bool matched = false;

        // Operator matching
        for (const auto &entry : sortedOps) {
            const std::string &opString = entry.first;
            const Token::Type type = entry.second;

            bool ok = true;
            for (size_t i = 0; i < opString.size(); i++) {
                if (peek(i) != opString[i]) {
                    ok = false;
                    break;
                }
            }
            if (!ok) continue;

            for (size_t i = 0; i < opString.size(); i++) consume();

            Token token{type, opString, tokenLine, tokenCol};
            if (binopMap.find(opString) != binopMap.end())
                token.binopValue = binopMap.at(opString);

            tokens.push_back(token);
            matched = true;
            break;
        }
        if (matched) continue;
        
        if (peek() == '+') {
            simplitiveBinOp(&tokens, "+", Token::Type::PLUS, tokenLine, tokenCol);
            continue;
        }
        if (peek() == '-') {
            simplitiveBinOp(&tokens, "-", Token::Type::MINUS, tokenLine, tokenCol);
            continue;
        }
        if (peek() == '@') {
            consume();
            if(peek(1) == ']' || peek(1) == ',' || peek(1) == ' ') {
                if(peek() == '-') {
                    consume();
                    selfUpd(&tokens, "-", Token::Type::MINUS, tokenLine, tokenCol, 1);
                    continue;
                }
                if (peek() == '+') {
                    consume();
                    selfUpd(&tokens, "+", Token::Type::PLUS, tokenLine, tokenCol, 1);
                    continue;
                }
            }
            tokens.push_back(Token{Token::Type::SELF_REFERENCE, "@", tokenLine, tokenCol});
            continue;
        }

        // String literal
        if (peek() == '"') {
            consume();
            std::string str;
            while (peek() && peek() != '"') {
                char c = consume();
                if (c == '\\') {
                    char next = consume();
                    switch (next) {
                        case 'n': str += '\n'; break;
                        case 't': str += '\t'; break;
                        case '\\': str += '\\'; break;
                        case '"': str += '"'; break;
                        default: str += next; break;
                    }
                } else {
                    str += c;
                }
            }
            if (peek() != '"') throw std::runtime_error("Unterminated string literal");
            consume();
            tokens.push_back(Token{Token::Type::STRING, str, tokenLine, tokenCol});
            continue;
        }

        // Number literal
        if (std::isdigit(peek()) || (peek() == '-' && std::isdigit(peek(1)))) {
            std::string number;
            if (peek() == '-') number += consume();
            bool decimal = false;
            char last = ' ';
            while (std::isdigit(peek()) || peek() == '.') {
                char c = peek();
                if (c == '.') {
                    if(peek() == '.') {
                        // it's a range
                        goto out;
                    }
                    if (decimal) {
                        throw std::runtime_error("Multiple decimal points in number");
                    }
                    decimal = true;
                }
                number += c;
                last = c;
                consume();
            }
            out:
            tokens.push_back(Token{Token::Type::NUMBER, number, tokenLine, tokenCol});
            continue;
        }

        if (std::isalpha(peek()) || peek() == '_') {
            std::string ident;
            while (std::isalnum(peek()) || peek() == '_') ident += consume();

            Token token{Token::Type::IDENTIFIER, ident, tokenLine, tokenCol};

            if (keywords.find(ident) != keywords.end()) {
                token.type = Token::Type::KEYWORD;
            } else if (primitives.find(ident) != primitives.end()) {
                token.type = Token::Type::PRIMITIVE;
                token.primitiveValue = primitives.at(ident);
            }

            tokens.push_back(token);
            continue;
        }

        throw std::runtime_error("Unexpected character: " + std::string(1, consume()));
    }

    tokens.push_back(Token{Token::Type::END_OF_FILE, "", lineIndex, colIndex});
    return tokens;
}