#include "lexer.hpp"
#include "token.hpp"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

using namespace box;

void test_basic_tokens() {
    std::cout << "Testing basic tokens..." << std::endl;
    
    std::string source = "( ) { } [ ] , ; : + - * / % ! != = == > >= < <=";
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    assert(tokens[0].type == TokenType::LPAREN);
    assert(tokens[1].type == TokenType::RPAREN);
    assert(tokens[2].type == TokenType::LBRACE);
    assert(tokens[3].type == TokenType::RBRACE);
    assert(tokens[4].type == TokenType::LBRACKET);
    assert(tokens[5].type == TokenType::RBRACKET);
    assert(tokens[6].type == TokenType::COMMA);
    assert(tokens[7].type == TokenType::SEMICOLON);
    assert(tokens[8].type == TokenType::COLON);
    assert(tokens[9].type == TokenType::PLUS);
    assert(tokens[10].type == TokenType::MINUS);
    assert(tokens[11].type == TokenType::STAR);
    assert(tokens[12].type == TokenType::SLASH);
    assert(tokens[13].type == TokenType::PERCENT);
    assert(tokens[14].type == TokenType::BANG);
    assert(tokens[15].type == TokenType::BANG_EQUAL);
    assert(tokens[16].type == TokenType::EQUAL);
    assert(tokens[17].type == TokenType::EQUAL_EQUAL);
    assert(tokens[18].type == TokenType::GREATER);
    assert(tokens[19].type == TokenType::GREATER_EQUAL);
    assert(tokens[20].type == TokenType::LESS);
    assert(tokens[21].type == TokenType::LESS_EQUAL);
    assert(tokens[22].type == TokenType::END_OF_FILE);
    
    std::cout << "✓ Basic tokens test passed!" << std::endl;
}

void test_keywords() {
    std::cout << "Testing keywords..." << std::endl;
    
    std::string source = "var print if else while for fun return true false nil and or";
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    assert(tokens[0].type == TokenType::VAR);
    assert(tokens[1].type == TokenType::PRINT);
    assert(tokens[2].type == TokenType::IF);
    assert(tokens[3].type == TokenType::ELSE);
    assert(tokens[4].type == TokenType::WHILE);
    assert(tokens[5].type == TokenType::FOR);
    assert(tokens[6].type == TokenType::FUN);
    assert(tokens[7].type == TokenType::RETURN);
    assert(tokens[8].type == TokenType::TRUE);
    assert(tokens[9].type == TokenType::FALSE);
    assert(tokens[10].type == TokenType::NIL);
    assert(tokens[11].type == TokenType::AND);
    assert(tokens[12].type == TokenType::OR);
    
    std::cout << "✓ Keywords test passed!" << std::endl;
}

void test_memory_keywords() {
    std::cout << "Testing memory-related keywords..." << std::endl;
    
    std::string source = "malloc free calloc realloc addr_of deref unsafe";
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    assert(tokens[0].type == TokenType::MALLOC);
    assert(tokens[1].type == TokenType::FREE);
    assert(tokens[2].type == TokenType::CALLOC);
    assert(tokens[3].type == TokenType::REALLOC);
    assert(tokens[4].type == TokenType::ADDR_OF);
    assert(tokens[5].type == TokenType::DEREF);
    assert(tokens[6].type == TokenType::UNSAFE);
    
    std::cout << "✓ Memory keywords test passed!" << std::endl;
}

void test_numbers() {
    std::cout << "Testing number literals..." << std::endl;
    
    std::string source = "42 3.14 1.5e10 2.0e-5 0.001";
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    assert(tokens[0].type == TokenType::NUMBER);
    assert(std::get<double>(tokens[0].literal) == 42.0);
    
    assert(tokens[1].type == TokenType::NUMBER);
    assert(std::get<double>(tokens[1].literal) == 3.14);
    
    assert(tokens[2].type == TokenType::NUMBER);
    assert(std::get<double>(tokens[2].literal) == 1.5e10);
    
    assert(tokens[3].type == TokenType::NUMBER);
    assert(std::get<double>(tokens[3].literal) == 2.0e-5);
    
    assert(tokens[4].type == TokenType::NUMBER);
    assert(std::get<double>(tokens[4].literal) == 0.001);
    
    std::cout << "✓ Number literals test passed!" << std::endl;
}

void test_strings() {
    std::cout << "Testing string literals..." << std::endl;
    
    std::string source = R"("hello" "world\n" "tab\there" "quote\"inside")";
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    assert(tokens[0].type == TokenType::STRING);
    assert(std::get<std::string>(tokens[0].literal) == "hello");
    
    assert(tokens[1].type == TokenType::STRING);
    assert(std::get<std::string>(tokens[1].literal) == "world\n");
    
    assert(tokens[2].type == TokenType::STRING);
    assert(std::get<std::string>(tokens[2].literal) == "tab\there");
    
    assert(tokens[3].type == TokenType::STRING);
    assert(std::get<std::string>(tokens[3].literal) == "quote\"inside");
    
    std::cout << "✓ String literals test passed!" << std::endl;
}

void test_string_escapes() {
    std::cout << "Testing string escape sequences..." << std::endl;
    
    std::string source = R"("\\backslash" "\x41BC" "\u0041test")";
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    assert(tokens[0].type == TokenType::STRING);
    assert(std::get<std::string>(tokens[0].literal) == "\\backslash");
    
    assert(tokens[1].type == TokenType::STRING);
    assert(std::get<std::string>(tokens[1].literal) == "ABC");
    
    assert(tokens[2].type == TokenType::STRING);
    assert(std::get<std::string>(tokens[2].literal) == "Atest");
    
    std::cout << "✓ String escape sequences test passed!" << std::endl;
}

void test_identifiers() {
    std::cout << "Testing identifiers..." << std::endl;
    
    std::string source = "myVar _private value123 _123abc";
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    assert(tokens[0].type == TokenType::IDENTIFIER);
    assert(tokens[0].lexeme == "myVar");
    
    assert(tokens[1].type == TokenType::IDENTIFIER);
    assert(tokens[1].lexeme == "_private");
    
    assert(tokens[2].type == TokenType::IDENTIFIER);
    assert(tokens[2].lexeme == "value123");
    
    assert(tokens[3].type == TokenType::IDENTIFIER);
    assert(tokens[3].lexeme == "_123abc");
    
    std::cout << "✓ Identifiers test passed!" << std::endl;
}

void test_comments() {
    std::cout << "Testing comments..." << std::endl;
    
    std::string source = R"(
        // Single line comment
        var x = 42; // End of line comment
        /* Block comment */
        var y = /* inline block */ 10;
        /* Nested /* comments */ work */
    )";
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    assert(tokens[0].type == TokenType::VAR);
    assert(tokens[1].type == TokenType::IDENTIFIER);
    assert(tokens[2].type == TokenType::EQUAL);
    assert(tokens[3].type == TokenType::NUMBER);
    assert(tokens[4].type == TokenType::SEMICOLON);
    assert(tokens[5].type == TokenType::VAR);
    
    std::cout << "✓ Comments test passed!" << std::endl;
}

void test_complex_program() {
    std::cout << "Testing complex program..." << std::endl;
    
    std::string source = R"(
        var x = 10;
        var y = 20.5;
        
        fun add(a, b) {
            return a + b;
        }
        
        if (x > 5) {
            print "x is greater than 5";
        } else {
            print "x is less or equal to 5";
        }
        
        var arr = [1, 2, 3];
        var dict = {1: 10, 2: 20};
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    assert(!tokens.empty());
    assert(tokens.back().type == TokenType::END_OF_FILE);
    
    std::cout << "✓ Complex program test passed!" << std::endl;
}

void test_arrow_operator() {
    std::cout << "Testing arrow operator..." << std::endl;
    
    std::string source = "ptr->field";
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    assert(tokens[0].type == TokenType::IDENTIFIER);
    assert(tokens[1].type == TokenType::ARROW);
    assert(tokens[2].type == TokenType::IDENTIFIER);
    
    std::cout << "✓ Arrow operator test passed!" << std::endl;
}

void test_io_keywords() {
    std::cout << "Testing IO keywords..." << std::endl;
    
    std::string source = "input input_num read_file write_file append_file file_exists";
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    assert(tokens[0].type == TokenType::INPUT);
    assert(tokens[1].type == TokenType::INPUT_NUM);
    assert(tokens[2].type == TokenType::READ_FILE);
    assert(tokens[3].type == TokenType::WRITE_FILE);
    assert(tokens[4].type == TokenType::APPEND_FILE);
    assert(tokens[5].type == TokenType::FILE_EXISTS);
    
    std::cout << "✓ IO keywords test passed!" << std::endl;
}

void test_error_unterminated_string() {
    std::cout << "Testing error: unterminated string..." << std::endl;
    
    std::string source = "\"unterminated";
    Lexer lexer(source);
    
    bool caught = false;
    try {
        lexer.scan_tokens();
    } catch (const std::runtime_error& e) {
        caught = true;
        std::string msg = e.what();
        assert(msg.find("Unterminated string") != std::string::npos);
    }
    
    assert(caught);
    std::cout << "✓ Unterminated string error test passed!" << std::endl;
}

void test_error_invalid_number() {
    std::cout << "Testing error: invalid number..." << std::endl;
    
    std::string source = "3.";
    Lexer lexer(source);
    
    bool caught = false;
    try {
        lexer.scan_tokens();
    } catch (const std::runtime_error& e) {
        caught = true;
        std::string msg = e.what();
        assert(msg.find("decimal point") != std::string::npos);
    }
    
    assert(caught);
    std::cout << "✓ Invalid number error test passed!" << std::endl;
}

void test_error_unexpected_character() {
    std::cout << "Testing error: unexpected character..." << std::endl;
    
    std::string source = "@invalid";
    Lexer lexer(source);
    
    bool caught = false;
    try {
        lexer.scan_tokens();
    } catch (const std::runtime_error& e) {
        caught = true;
        std::string msg = e.what();
        assert(msg.find("Unexpected character") != std::string::npos);
    }
    
    assert(caught);
    std::cout << "✓ Unexpected character error test passed!" << std::endl;
}

void test_multiline_string() {
    std::cout << "Testing multiline string..." << std::endl;
    
    std::string source = R"("line1
line2
line3")";
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    assert(tokens[0].type == TokenType::STRING);
    std::string value = std::get<std::string>(tokens[0].literal);
    assert(value.find("line1") != std::string::npos);
    assert(value.find("line2") != std::string::npos);
    assert(value.find("line3") != std::string::npos);
    
    std::cout << "✓ Multiline string test passed!" << std::endl;
}

void test_switch_case() {
    std::cout << "Testing switch-case keywords..." << std::endl;
    
    std::string source = "switch case default break";
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    assert(tokens[0].type == TokenType::SWITCH);
    assert(tokens[1].type == TokenType::CASE);
    assert(tokens[2].type == TokenType::DEFAULT);
    assert(tokens[3].type == TokenType::BREAK);
    
    std::cout << "✓ Switch-case keywords test passed!" << std::endl;
}

void test_ptr_is_identifier() {
    std::cout << "Testing that 'ptr' is an identifier, not a keyword..." << std::endl;
    
    std::string source = "ptr";
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    assert(tokens[0].type == TokenType::IDENTIFIER);
    assert(tokens[0].lexeme == "ptr");
    
    std::cout << "✓ PTR identifier test passed!" << std::endl;
}

void test_all_tokens_printed() {
    std::cout << "\n=== Testing token printing ===" << std::endl;
    
    std::string source = "var x = 42; print \"hello\";";
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    
    std::cout << "Tokens generated:" << std::endl;
    for (const auto& token : tokens) {
        std::cout << "  " << token << std::endl;
    }
    std::cout << "=== End of token printing ===\n" << std::endl;
}

int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "BOX LEXER COMPREHENSIVE TEST SUITE" << std::endl;
    std::cout << std::string(70, '=') << "\n" << std::endl;
    
    try {
        test_basic_tokens();
        test_keywords();
        test_memory_keywords();
        test_numbers();
        test_strings();
        test_string_escapes();
        test_identifiers();
        test_comments();
        test_arrow_operator();
        test_io_keywords();
        test_switch_case();
        test_ptr_is_identifier();
        test_complex_program();
        test_multiline_string();
        
        test_error_unterminated_string();
        test_error_invalid_number();
        test_error_unexpected_character();
        
        test_all_tokens_printed();
        
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "ALL TESTS PASSED! ✓✓✓" << std::endl;
        std::cout << std::string(70, '=') << "\n" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "TEST FAILED!" << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}