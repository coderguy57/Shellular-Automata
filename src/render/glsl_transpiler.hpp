#pragma once

#include <exception>
#include <string>
#include <vector>

// Props to wxwisiasdf for making the transpiler https://github.com/wxwisiasdf
namespace GLSL {
    enum class TokenType {
        ASSIGN,
        TERNARY,
        LITERAL,
        IDENTIFIER,
        ADD,
        SUB,
        MUL,
        DIV,
        REM,
        AND,
        OR,
        XOR,
        SEMICOLON,
        COMMA,
        COLON,
        DOT,
        CMP_EQ,
        CMP_NEQ,
        CMP_GT,
        CMP_LT,
        CMP_GTEQ,
        CMP_LTEQ,
        CMP_OR,
        CMP_AND,
        LPAREN,
        RPAREN,
        LBRACKET,
        RBRACKET,
        LBRACE,
        RBRACE,
        NEWLINE,

        // Special "hacky" stuff
        MACRO,
    };

    class Token {
    public:
        Token(TokenType _type) : type(_type){};
        ~Token(){};

        enum TokenType type;
        std::string data;
    };

    enum class VariableType {
        LOCAL,
        PROVIDED,
        INPUT,
        OUTPUT,
    };

    class Variable {
    public:
        enum VariableType type;
        std::string type_name;
        std::string name;
        bool is_const;
        int layout_n;
    };

    class Function {
    public:
        std::string name;
        std::vector<std::pair<std::string, std::string>> args;
        std::string ret_type;
    };

    class Define {
    public:
        std::string name;
        std::string value;
    };

    class Context {
    public:
        Context(const std::string& buffer);
        ~Context();
        std::string get_identifier(std::string::iterator& it);
        std::string get_literal(std::string::iterator& it);
        void lexer();
        void parser();
        std::string to_text();

        std::vector<int> line_numbers;
        std::vector<Variable> vars;
        std::vector<Function> funcs;
        std::vector<Token> tokens;
        std::vector<Define> defines;
        std::string buffer;
    };

    class Exception : public std::exception {
        std::string buffer;

    public:
        Exception(std::vector<Token>::iterator _it, const std::string& _buffer) : buffer(_buffer), it(_it){};
        virtual const char* what() const noexcept {
            return buffer.c_str();
        }

        std::vector<Token>::iterator it;
    };
};