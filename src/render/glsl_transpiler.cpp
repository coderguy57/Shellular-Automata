#include "glsl_transpiler.hpp"

#include <sstream>

using namespace GLSL;

Context::Context(const std::string &_buffer)
    : buffer(_buffer) {
}

Context::~Context() {
}

IOption::IOption(Type type_, std::string name_, std::string label_, std::vector<Token>::iterator it, bool compile_constant_)
    : name{name_}, label{label_}, type{type_}, compile_constant{compile_constant_}, _it{it} {}

BoolOption::BoolOption(std::string name, std::string label, std::vector<Token>::iterator it, bool default_value, bool compile_constant)
    : value{default_value}, IOption(IOption::Type::Bool, name, label, it, compile_constant) {}

CommandOption::CommandOption(std::string name, std::string label, std::vector<Token>::iterator it, bool compile_constant)
    : IOption(IOption::Type::Command, name, label, it, compile_constant) {}

template class ValueOption<int>;
template class ValueOption<u_int>;
template class ValueOption<float>;

template <>
ValueOption<int>::ValueOption(std::string name, std::string label, std::vector<Token>::iterator it, int default_value, int min_, int max_, bool compile_constant)
    : value{default_value}, min{min_}, max{max_}, IOption(IOption::Type::Int, name, label, it, compile_constant) {}


template <>
ValueOption<u_int>::ValueOption(std::string name, std::string label, std::vector<Token>::iterator it, u_int default_value, u_int min_, u_int max_, bool compile_constant)
    : value{default_value}, min{min_}, max{max_}, IOption(IOption::Type::UInt, name, label, it, compile_constant) {}


template <>
ValueOption<float>::ValueOption(std::string name, std::string label, std::vector<Token>::iterator it, float default_value, float min_, float max_, bool compile_constant)
    : value{default_value}, min{min_}, max{max_}, IOption(IOption::Type::Float, name, label, it, compile_constant) {}

template <>
std::string ValueOption<int>::const_expression() {
    return "const int " + name + " = " + std::to_string(value) + ";";
}

template <>
std::string ValueOption<u_int>::const_expression() {
    return "const uint " + name + " = " + std::to_string(value) + ";";
}

template <>
std::string ValueOption<float>::const_expression() {
    return "const float " + name + " = " + std::to_string(value) + ";";
}

// void Option::update_option() {
//     if (!compile_constant) {
//         _it->data = "";
//         return;
//     }

//     switch (type) {
//         case Type::Float:
//             _it->data = "const float " + name + " " + std::to_string(value) + ";";
//             break;
//         case Type::Int:
//             _it->data = "const int " + name + " " + std::to_string((int)value) + ";";
//             break;
//         case Type::UInt:
//             _it->data = "const uint " + name + " " + std::to_string((u_int)value) + ";";
//             break;
//         case Type::Bool:
//         case Type::Command:
//             _it->data = "const bool " + name + " " + (value ? "true" : "false") + ";";
//     }
// }

std::string Context::get_identifier(std::string::iterator &it) {
    std::string::iterator start_it = it;

    // Alphanumerics, _ and dots are allowed as identifiers
    while (it != buffer.end() && (isalnum(*it) || *it == '_' || *it == '.'))
        it++;

    std::string str = buffer.substr(std::distance(buffer.begin(), start_it), std::distance(start_it, it));
    return str;
}

std::string Context::get_literal(std::string::iterator &it) {
    std::string::iterator start_it = it;

    // Literal
    while (it != buffer.end() && (isdigit(*it) || *it == '.'))
        it++;

    // Skip 'type' specifier
    while (it != buffer.end() && isalpha(*it))
        it++;
    // if(it != buffer.end() && isalpha(*it))
    //     it++;

    std::string str = buffer.substr(std::distance(buffer.begin(), start_it), std::distance(start_it, it));
    return str;
}

void Context::lexer() {
    // Output the final stuff
    std::string::iterator it = buffer.begin();
    for (; it != buffer.end();) {
        while (it != buffer.end() && isspace(*it) && (*it == '\r'))
            it++;

        if (it == buffer.end())
            break;

        if (*it == '\n') {
            tokens.push_back(Token(TokenType::NEWLINE));
            it++;
        } else if ((*(it + 0) == '/' && *(it + 1) == '/')) {
            it += 2;
            // Single-line comments
            bool command = (*it == '!');
            auto start_it = it + 1;
            while (it != buffer.end() && (*it != '\n'))
                it++;

            if (command) {
                Token tok = Token(TokenType::COMMAND);
                tok.data = buffer.substr(std::distance(buffer.begin(), start_it), std::distance(start_it, it));
                tokens.push_back(tok);
            }
        } else if (*(it + 0) == '/' && *(it + 1) == '*') {
            it += 2;
            // Multiline comments
            while ((it + 1) != buffer.end()) {
                if (*(it + 0) == '*' && *(it + 1) == '/') {
                    it += 2;
                    break;
                } else if (*it == '\n') {
                    tokens.push_back(Token(TokenType::NEWLINE));
                }
                it++;
            }
        } else if (*it == '#') {
            it++;
            auto start_it = it;
            while (it != buffer.end() && (*it != '\n'))
                it++;

            Token tok = Token(TokenType::MACRO);
            tok.data = buffer.substr(std::distance(buffer.begin(), start_it), std::distance(start_it, it));
            tokens.push_back(tok);
        } else if (*it == ',') {
            tokens.push_back(Token(TokenType::COMMA));
            it++;
        } else if (*it == ';') {
            tokens.push_back(Token(TokenType::SEMICOLON));
            it++;
        } else if (*it == '(') {
            tokens.push_back(Token(TokenType::LPAREN));
            it++;
        } else if (*it == ')') {
            tokens.push_back(Token(TokenType::RPAREN));
            it++;
        } else if (*it == '[') {
            tokens.push_back(Token(TokenType::LBRACKET));
            it++;
        } else if (*it == ']') {
            tokens.push_back(Token(TokenType::RBRACKET));
            it++;
        } else if (*it == '{') {
            tokens.push_back(Token(TokenType::LBRACE));
            it++;
        } else if (*it == '}') {
            tokens.push_back(Token(TokenType::RBRACE));
            it++;
        } else if (*it == '+') {
            tokens.push_back(Token(TokenType::ADD));
            it++;
        } else if (*it == '-') {
            tokens.push_back(Token(TokenType::SUB));
            it++;
        } else if (*it == '*') {
            tokens.push_back(Token(TokenType::MUL));
            it++;
        } else if (*it == '/') {
            tokens.push_back(Token(TokenType::DIV));
            it++;
        } else if (*it == '%') {
            tokens.push_back(Token(TokenType::REM));
            it++;
        } else if (*it == '^') {
            tokens.push_back(Token(TokenType::XOR));
            it++;
        } else if (*it == '<') {
            it++;
            if (it != buffer.end() && *it == '=') {
                tokens.push_back(Token(TokenType::CMP_LTEQ));
                it++;
            } else {
                tokens.push_back(Token(TokenType::CMP_LT));
            }
        } else if (*it == '>') {
            it++;
            if (it != buffer.end() && *it == '=') {
                tokens.push_back(Token(TokenType::CMP_GTEQ));
                it++;
            } else {
                tokens.push_back(Token(TokenType::CMP_GT));
            }
        } else if (*it == '|') {
            it++;
            if (it != buffer.end() && *it == '|') {
                tokens.push_back(Token(TokenType::CMP_OR));
                it++;
            } else {
                tokens.push_back(Token(TokenType::OR));
            }
        } else if (*it == '&') {
            it++;
            if (it != buffer.end() && *it == '&') {
                tokens.push_back(Token(TokenType::CMP_AND));
                it++;
            } else {
                tokens.push_back(Token(TokenType::AND));
            }
        } else if (*it == '&') {
            it++;
            if (it != buffer.end() && *it == '&') {
                tokens.push_back(Token(TokenType::CMP_AND));
                it++;
            } else {
                tokens.push_back(Token(TokenType::AND));
            }
        } else if (*it == '=') {
            it++;
            if (it != buffer.end() && *it == '=') {
                tokens.push_back(Token(TokenType::CMP_EQ));
                it++;
            } else {
                tokens.push_back(Token(TokenType::ASSIGN));
            }
        } else if (*it == '?') {
            tokens.push_back(Token(TokenType::TERNARY));
            it++;
        } else if (*it == ':') {
            tokens.push_back(Token(TokenType::COLON));
            it++;
        } else if (*it == '.') {
            tokens.push_back(Token(TokenType::DOT));
            it++;
        } else if (*it == '!' && (it + 1) != buffer.end() && *(it + 1) == '=') {
            tokens.push_back(Token(TokenType::CMP_NEQ));
            it += 2;
        } else {
            if (isdigit(*it) || *it == '.') {
                Token tok = Token(TokenType::LITERAL);
                tok.data = get_literal(it);
                tokens.push_back(tok);
            } else if (isalnum(*it) || *it == '_') {
                Token tok = Token(TokenType::IDENTIFIER);
                tok.data = get_identifier(it);
                tokens.push_back(tok);
            } else {
                it++;
            }
        }
    }
}

IOption::Type get_option_type(std::vector<Token>::iterator it) {
    std::string type_name = it->data;
    if (type_name == "float") {
        return IOption::Type::Float;
    } else if (type_name == "bool") {
        return IOption::Type::Bool;
    } else if (type_name == "int") {
        return IOption::Type::Int;
    } else if (type_name == "uint") {
        return IOption::Type::UInt;
    } else {
        throw Exception(it, "Option type not accepted");
    }
}

void Context::parser() {
    // Im not super happy with how the options work, will have to change this at some point
    for (auto it = tokens.begin(); it != tokens.end(); it++) {
        if (it->type != TokenType::COMMAND) {
            continue;
        }

        std::istringstream params(it->data);
        std::string param;
        params >> param;
        if (param != "option") {
            continue;
        }

        if ((it + 2)->data != "const" && (it + 2)->data != "uniform") {
            throw Exception((it + 2), "Option must be followed by either a const or an uniform.");
        }
        bool compile_const = false;
        if ((it + 2)->data == "const") {
            compile_const = true;
        }
        IOption::Type type = get_option_type(it + 3);
        std::string name = (it + 4)->data;

        std::string default_value;
        if ((it + 5)->type == TokenType::ASSIGN) {
            default_value = (it + 6)->data;
        }

        it->type = TokenType::OPTION;

        std::string label = name;
        float min = 0, max = 1;
        while (params) {
            char delimiter;
            params >> delimiter;
            if (delimiter == '\"') {
                params >> name >> param;
            } else if (delimiter == '(') {
                if (type == IOption::Type::Bool) {
                    params >> param;
                    type = param == "command)" ? IOption::Type::Command : type;
                    delimiter = ')';
                    continue;
                }
                params >> min;
                if (params.peek() == ',')
                    params >> delimiter;
                params >> max;
                params >> delimiter;
                if (params.fail() || delimiter != ')') {
                    throw Exception(it, "Option range is invalid.");
                }
            } else if (delimiter == '=') {
                params >> default_value;
            }
        }

        switch (type) {
            case IOption::Type::Float:
                options.push_back(new ValueOption<float>(name, label, it, std::stof(default_value), min, max, compile_const));
                break;
            case IOption::Type::Int:
                options.push_back(new ValueOption<int>(name, label, it, std::stoi(default_value), min, max, compile_const));
                break;
            case IOption::Type::UInt:
                options.push_back(new ValueOption<u_int>(name, label, it, std::stoi(default_value), min, max, compile_const));
                break;
            case IOption::Type::Bool:
                options.push_back(new BoolOption(name, label, it, default_value == "true", compile_const));
                break;
            case IOption::Type::Command:
                options.push_back(new CommandOption(name, label, it, compile_const));
                break;
            default:
                break;
        }

        options.back()->update_option();
        it = it + 2;
        // Remove the next tokens if the option is a compile constant
        // The option will be used instead
        if (compile_const) {
            auto start_it = it;
            while (it != tokens.end() && (it->type != TokenType::NEWLINE))
                it++;
            it = tokens.erase(start_it, it);
        }
    }
}

std::string Context::to_text() {
    std::vector<Token>::const_iterator it = tokens.begin();
    std::string end_buffer;
    int current_line = 1;

    // Go after the first instance of a preprocessor macro
    if (it->type == TokenType::MACRO) {
        end_buffer += "#" + it->data + "\r\n";
        line_numbers.push_back(current_line++);
        it += 2;  // Skip the NEWLINE also
        for (const auto option : options) {
            option->update_option();
        }
    }

    for (; it != tokens.end(); it++) {
        switch (it->type) {
            case TokenType::MACRO:
                end_buffer += "#" + it->data;
                break;
            case TokenType::NEWLINE:
                line_numbers.push_back(current_line++);
                end_buffer += "\n";
                break;
            case TokenType::OPTION:
                end_buffer += it->data;
                break;
            case TokenType::SEMICOLON:
                end_buffer += ";";
                break;
            case TokenType::COMMA:
                end_buffer += ",";
                break;
            case TokenType::LPAREN:
                end_buffer += "(";
                break;
            case TokenType::RPAREN:
                end_buffer += ")";
                break;
            case TokenType::LBRACKET:
                end_buffer += "[";
                break;
            case TokenType::RBRACKET:
                end_buffer += "]";
                break;
            case TokenType::LBRACE:
                end_buffer += "{";
                break;
            case TokenType::RBRACE:
                end_buffer += "}";
                break;
            case TokenType::ADD:
                end_buffer += "+";
                break;
            case TokenType::SUB:
                end_buffer += "-";
                break;
            case TokenType::MUL:
                end_buffer += "*";
                break;
            case TokenType::DIV:
                end_buffer += "/";
                break;
            case TokenType::REM:
                end_buffer += "%";
                break;
            case TokenType::XOR:
                end_buffer += "^";
                break;
            case TokenType::CMP_AND:
                end_buffer += "&&";
                break;
            case TokenType::AND:
                end_buffer += "&";
                break;
            case TokenType::CMP_OR:
                end_buffer += "||";
                break;
            case TokenType::OR:
                end_buffer += "|";
                break;
            case TokenType::CMP_LT:
                end_buffer += "<";
                break;
            case TokenType::CMP_LTEQ:
                end_buffer += "<=";
                break;
            case TokenType::CMP_GT:
                end_buffer += ">";
                break;
            case TokenType::CMP_GTEQ:
                end_buffer += ">=";
                break;
            case TokenType::TERNARY:
                end_buffer += "?";
                break;
            case TokenType::COLON:
                end_buffer += ":";
                break;
            case TokenType::DOT:
                end_buffer += ".";
                break;
            case TokenType::LITERAL:
                end_buffer += it->data;
                break;
            case TokenType::IDENTIFIER:
                if (it->data == "layout")
                    end_buffer += it->data + " ";
                else
                    end_buffer += " " + it->data + " ";
                break;
            case TokenType::ASSIGN:
                end_buffer += "=";
                break;
            case TokenType::CMP_EQ:
                end_buffer += "==";
                break;
            case TokenType::CMP_NEQ:
                end_buffer += "!=";
                break;
            default:
                break;
        }
    }

    return end_buffer;
}