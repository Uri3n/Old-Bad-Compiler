//
// Created by Diago on 2024-07-02.
//

#include <lexer.hpp>


std::string
lexer_token_type_to_string(token_t type) {

    switch(type) {
        case TOKEN_NONE:             return "None";
        case END_OF_FILE:            return "End Of File";
        case ILLEGAL:                return "Illegal";
        case IDENTIFIER:             return "Identifier";
        case VALUE_ASSIGNMENT:       return "Value Assignment";
        case TYPE_ASSIGNMENT:        return "Type Assignment";
        case CONST_TYPE_ASSIGNMENT:  return "Const Type Assignment";
        case SEMICOLON:              return "Semicolon";
        case LPAREN:                 return "Left Parenthesis";
        case RPAREN:                 return "Right Parenthesis";
        case LBRACE:                 return "left Brace";
        case RBRACE:                 return "Right Brace";
        case LSQUARE_BRACKET:        return "Left Square Bracket";
        case RSQUARE_BRACKET:        return "Right Square Bracket";
        case COMMA:                  return "Comma";
        case DOT:                    return "Dot";
        case QUESTION_MARK:          return "Question Mark";
        case POUND:                  return "Pound Sign";
        case AT:                     return "At Symbol";
        case COMP_EQUALS:            return "Comparison-Equals";
        case COMP_NOT_EQUALS:        return "Comparison-Not-Equals";
        case COMP_LT:                return "Less-Than";
        case COMP_LTE:               return "Less-Than-Or-Equals";
        case COMP_GT:                return "Greater-Than";
        case COMP_GTE:               return "Greater-Than-Or-Equals";
        case CONDITIONAL_AND:        return "Conditional AND";
        case CONDITIONAL_OR:         return "Conditional OR";
        case CONDITIONAL_NOT:        return "Conditional NOT";
        case INTEGER_LITERAL:        return "Integer Literal";
        case FLOAT_LITERAL:          return "Float Literal";
        case STRING_LITERAL:         return "String Literal";
        case BOOLEAN_LITERAL:        return "Boolean Literal";
        case CHARACTER_LITERAL:      return "Character Literal";
        case PLUS:                   return "Addition";
        case PLUSEQ:                 return "Addition-Equals";
        case SUB:                    return "Subtraction";
        case SUBEQ:                  return "Subtraction-Equals";
        case MUL:                    return "Multiplication";
        case MULEQ:                  return "Multiplication-Equals";
        case DIV:                    return "Division";
        case DIVEQ:                  return "Division-Equals";
        case MOD:                    return "Modulus";
        case MODEQ:                  return "Modulus-Equals";
        case BITWISE_AND:            return "Bitwise AND";
        case BITWISE_ANDEQ:          return "Bitwise AND-Equals";
        case BITWISE_NOT:            return "Bitwise NOT";
        case BITWISE_OR:             return "Bitwise OR";
        case BITWISE_OREQ:           return "Bitwise OR-Equals";
        case BITWISE_XOR_OR_PTR:     return "Bitwise XOR Or Pointer";
        case BITWISE_XOREQ:          return "Bitwise XOR-Equals";
        case BITWISE_LSHIFT:         return "Left Bitshift";
        case BITWISE_RSHIFT:         return "Right Bitshift";
        case KW_RET:                 return R"(Keyword "ret")";
        case KW_BRK:                 return R"(Keyword "brk")";
        case KW_CONT:                return R"(Keyword "cont")";
        case KW_FOR:                 return R"(Keyword "for")";
        case KW_WHILE:               return R"(Keyword "while")";
        case KW_DO:                  return R"(Keyword "do")";
        case KW_IF:                  return R"(Keyword "if")";
        case KW_ELSE:                return R"(Keyword "else")";
        case KW_ELIF:                return R"(Keyword "elif")";
        case TOKEN_KW_PROC:          return R"(Keyword "proc")";
        case KW_STRUCT:              return R"(Keyword "struct")";
        case KW_SWITCH:              return R"(Keyword "switch")";
        case KW_CASE:                return R"(Keyword "case")";
        case KW_ENUM:                return R"(Keyword "enum")";
        case TOKEN_KW_VOID:          return R"(Keyword "void")";
        case ARROW:                  return "Arrow";
        case TOKEN_KW_F64:           return "64-bit Float Type-Identifier";
        case TOKEN_KW_F32:           return "32-bit Float Type-Identifier";
        case TOKEN_KW_U8:            return "Unsigned 8-Bit Integer Type-Identifier";
        case TOKEN_KW_I8:            return "Signed 8-Bit Integer Type-Identifier";
        case TOKEN_KW_U16:           return "Unsigned 16-Bit Integer Type-Identifier";
        case TOKEN_KW_I16:           return "Signed 16-Bit Integer Type-Identifier";
        case TOKEN_KW_U32:           return "Unsigned 32-Bit Integer Type-Identifier";
        case TOKEN_KW_I32:           return "Signed 32-Bit Integer Type-Identifier";
        case TOKEN_KW_U64:           return "Unsigned 64-Bit Integer Type-Identifier";
        case TOKEN_KW_I64:           return "Signed 64-Bit Integer Type-Identifier";
        case TOKEN_KW_BOOL:          return "Boolean Type Identifier";
        default:                     return "Unknown";
    }
}


std::string
lexer_token_kind_to_string(const token_kind kind) {

    switch(kind) {
        case UNSPECIFIC:            return "Unspecific";
        case PUNCTUATOR:            return "Punctuator";
        case BINARY_EXPR_OPERATOR:  return "Binary Operator";
        case UNARY_EXPR_OPERATOR:   return "Unary Operator";
        case LITERAL:               return "Literal";
        case KEYWORD:               return "Keyword";
        case TYPE_IDENTIFIER:       return "Type Identifier";
        default:                    return "Unknown";
    }
}


void
lexer_display_token_data(const token& tok) {
    print(
        "Value: {}\nType: {}\nKind: {}\nFile Pos Index: {}\nLine Number: {}\n",
        tok.value,
        lexer_token_type_to_string(tok.type),
        lexer_token_kind_to_string(tok.kind),
        tok.src_pos,
        tok.line
    );
}


void
lexer::advance_char(const uint32_t amnt) {
    if(_current.type != END_OF_FILE && src_index < src.size()) {
        src_index += amnt;
    }
}


void
lexer::advance_line() {
    ++curr_line;
}


char
lexer::peek_char() {
    if(src_index + 1 >= src.size()) {
        return '\0';
    }

    return src[src_index + 1];
}


char
lexer::current_char() {
    if(src_index >= src.size()) {
        return '\0';
    }

    return src[src_index];
}


std::optional<char>
get_escaped_char_via_real(const char real) {

    switch(real) {
        case 'n':  return '\n';
        case 'b':  return '\b';
        case 'a':  return '\a';
        case 'r':  return '\r';
        case '\'': return '\'';
        case '\\': return '\\';
        case '"':  return '"';
        case 't':  return '\t';
        case '0':  return '\0';
        default:   return std::nullopt;
    }

}


std::optional<std::string>
remove_escaped_chars(const std::string_view& str) {

    std::string buffer;

    for(size_t i = 0; i < str.size(); i++) {
        if(str[i] == '\\') {
            if(i + 1 >= str.size()) {
                return std::nullopt;
            }

            if(const auto escaped = get_escaped_char_via_real(str[i + 1])) {
                buffer += *escaped;
                i++;
            } else {
                return std::nullopt;
            }
        }
    }

    return buffer;
}


bool
lexer::init(const std::string& file_name) {

    std::ifstream input(file_name, std::ios::binary);
    source_file_name = file_name;

    auto _ = defer([&] {
       if(input.is_open()) {
           input.close();
       }
    });


    if(!input.is_open()) {
        print("FATAL, could not open source file \"{}\".", file_name);
        return false;
    }


    input.seekg(0, std::ios::end);
    const std::streamsize file_size = input.tellg();
    input.seekg(0, std::ios::beg);


    src.resize(file_size);
    if(!input.read(src.data(), file_size)) {
        print("FATAL, opened source file but contents could not be read.");
        return false;
    }

    return true;
}


bool
lexer::init(const std::vector<char>& src) {
    this->src = src;
    return true;
}
