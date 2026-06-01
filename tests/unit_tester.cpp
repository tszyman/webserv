#include "parser/RequestParser.hpp"
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include "network/SocketEngine.hpp"

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
        std::cerr << "Usage: ./unit_tester [component] [args...]" << std::endl;
        return 1;
    }

    std::string component = argv[1];

    // ==========================================
    // COMPONENT: REQUEST PARSER
    // ==========================================
    if (component == "parser") {
        if (argc < 3) {
            std::cerr << "Error: 'parser' requires a payload argument." << std::endl;
            return 1;
        }
        std::string raw_payload = argv[2];
        RequestParser parser;
        parser.feed(raw_payload.c_str(), raw_payload.length());
        print_parser_result(parser);
        return 0;
    }

    // ==========================================
    // COMPONENT: SOCKET ENGINE
    // ==========================================
else if (component == "socket") {
        int port = (argc >= 3) ? std::atoi(argv[2]) : 8080;
        SocketEngine engine(port); 
        
        // FIX: Wrap init() in a try-catch block to handle exceptions properly
        try {
            engine.init();
            std::cout << "SUCCESS_SOCKET_INIT" << std::endl;
        } 
        catch (const std::exception& e) {
            // Log the actual exception message to stderr for debugging
            std::cerr << "SocketEngine exception caught: " << e.what() << std::endl;
            // Print the failure token to stdout for the Python tester
            std::cout << "FAILED_SOCKET_INIT" << std::endl;
        }
        catch (...) {
            // Catch-all for non-standard exceptions
            std::cerr << "SocketEngine unknown exception thrown." << std::endl;
            std::cout << "FAILED_SOCKET_INIT" << std::endl;
        }
        return 0;
    }

    // ==========================================
    // COMPONENT: CONNECTION (Placeholder)
    // ==========================================
    else if (component == "connection") {
        // Implement connection testing logic here later
        std::cout << "CONNECTION_TEST_PLACEHOLDER" << std::endl;
        return 0;
    }

    std::cerr << "Unknown component: " << component << std::endl;
    return 1;
}