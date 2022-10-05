#pragma once

#include <exception>
#include <algorithm>
#include <string>
#include <vector>

// Props to wxwisiasdf for making the transpiler https://github.com/wxwisiasdf
namespace GLSL
{
    enum class TokenType
    {
        ASSIGN, TERNARY, LITERAL, IDENTIFIER,
        ADD, SUB, MUL, DIV, REM, AND, OR, XOR,
        SEMICOLON, COMMA, COLON, DOT,
        CMP_EQ, CMP_NEQ, CMP_GT, CMP_LT, CMP_GTEQ, CMP_LTEQ, CMP_OR, CMP_AND,
        LPAREN, RPAREN, LBRACKET, RBRACKET, LBRACE, RBRACE, NEWLINE,
        MACRO,

        // Special "hacky" stuff
        COMMAND,
        OPTION,
    };

    class Token
    {
    public:
        Token(TokenType _type) : type(_type){};
        ~Token(){};

        enum TokenType type;
        std::string data;
    };

    class IOption
    {
    public:
        enum class Type
        {
            Float,
            Int,
            UInt,
            Bool,
            Command, // Only active for one update. Example uses like clearing the screen
        };

        const bool compile_constant;
        const Type type;
        const std::string name;
        const std::string label;

        inline void update_option() { _it->data = (compile_constant ? const_expression() : ""); }

        bool changed = false;
    protected:
        virtual std::string const_expression() = 0;
        IOption(Type type, std::string name, std::string label, std::vector<Token>::iterator it, bool compile_constant);
        std::vector<Token>::iterator _it;
    };

    template <class T>
    class ValueOption : public IOption
    {
    public:
        ValueOption(std::string name, std::string label, std::vector<Token>::iterator it, T default_value, T min, T max, bool compile_constant, bool logarithmic);

        const T min;
        const T max;
        const bool logarithmic = false;

        inline T get_value() {return value; }
        std::string const_expression() override;
        T value;
    private:
    };

    class BoolOption : public IOption
    {
    public:
        BoolOption(std::string name, std::string label, std::vector<Token>::iterator it, bool default_value, bool compile_constant);

        inline std::string const_expression() override { return "const bool " + name + " = " + (value ? "true;" : "false;"); }
        bool value;
    private:
    };

    class CommandOption : public IOption
    {
    public:
        CommandOption(std::string name, std::string label, std::vector<Token>::iterator it, bool compile_constant);

        inline void activate() { _value = true; changed = true; };
        inline std::string const_expression() override { return "const bool " + name + " = " + (_value ? "true;" : "false;"); }
    private:
        bool _value;
    };

    class Define
    {
    public:
        std::string name;
        std::string value;
    };

    class Context
    {
    public:
        Context(const std::string &buffer);
        ~Context();
        std::string get_identifier(std::string::iterator &it);
        std::string get_literal(std::string::iterator &it);
        void lexer();
        void parser();
        std::string to_text();

        std::vector<int> line_numbers;
        std::vector<Token> tokens;
        std::vector<IOption*> options;
        std::string buffer;
    };

    class Exception : public std::exception
    {
        std::string _buffer;

    public:
        Exception(std::vector<Token>::iterator it_, const std::string& buffer) : _buffer(buffer), it(it_){};
        virtual const char* what() const noexcept {
            return _buffer.c_str();
        }

        std::vector<Token>::iterator it;
    };
};