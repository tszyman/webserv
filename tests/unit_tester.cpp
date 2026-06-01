#include "parser/RequestParser.hpp"
#include <iostream>
#include <string>
#include <map>
#include <vector>

/* 

This is the part of the tester that is able to test specific C++ classes if needed
, separately from the whole server running.

*/

void print_parser_result(const RequestParser &parser) {
    if (parser.getState() == RequestParser::STATE_COMPLETE) {
        std::cout << "SUCCESS" << std::endl;
        std::cout << "Method: " << parser.getMethod() << std::endl;
        std::cout << "Path: " << parser.getPath() << std::endl;
        std::cout << "Version: " << parser.getVersion() << std::endl;
        
        // Print Headers
        std::map<std::string, std::string> headers = parser.getHeaders();
        std::map<std::string, std::string>::const_iterator it;
        for (it = headers.begin(); it != headers.end(); ++it) {
            std::cout << "H-" << it->first << ": " << it->second << std::endl;
        }

        // Print Body Content
        const std::vector<char> &body = parser.getBody();
        std::cout << "Body: ";
        for (size_t i = 0; i < body.size(); ++i) {
            std::cout << body[i];
        }
        std::cout << std::endl;
    } else if (parser.getState() == RequestParser::STATE_ERROR) {
        std::cout << "FAILED" << std::endl;
    } else {
        std::cout << "INCOMPLETE (State code: " << parser.getState() << ")" << std::endl;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./unit_tester [raw_http_payload]" << std::endl;
        return 1;
    }

    // Combine all arguments into a single raw HTTP payload string
    std::string raw_payload = argv[1];
    
    RequestParser parser;
    // Feed the raw data directly into your parser stream engine
    parser.feed(raw_payload.c_str(), raw_payload.length());

    print_parser_result(parser);
    return 0;
}