#include "glsl_transpiler.hpp"

using namespace GLSL;
//
// GLSL Context
//
Context::Context(const std::string& _buffer)
    : buffer(_buffer) {
}

Context::~Context() {
}

std::string Context::get_identifier(std::string::iterator& it) {
    std::string::iterator start_it = it;

    // Alphanumerics, _ and dots are allowed as identifiers
    while (it != buffer.end() && (isalnum(*it) || *it == '_' || *it == '.'))
        it++;

    std::string str = buffer.substr(std::distance(buffer.begin(), start_it), std::distance(start_it, it));
    return str;
}

std::string Context::get_literal(std::string::iterator& it) {
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

        if (it == buffer.end()) break;

        if (*it == '\n') {
            tokens.push_back(Token(TokenType::NEWLINE));
            it++;
        } else if ((*(it + 0) == '/' && *(it + 1) == '/')) {
            it += 2;
            // Single-line comments
            while (it != buffer.end() && (*it != '\n'))
                it++;
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

void Context::parser() {
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
        for (const auto& define : defines) {
            end_buffer += "#define " + define.name + " " + define.value + "\r\n";
            line_numbers.push_back(current_line);
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
                else if (it->data == "provided")
                    end_buffer += " uniform ";
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