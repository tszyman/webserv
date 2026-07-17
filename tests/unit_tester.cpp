#include "parser/RequestParser.hpp"
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include "network/SocketEngine.hpp"
#include "network/EventLoop.hpp"
#include "http/StatusCodes.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpErrorPage.hpp"
#include "http/HttpRequest.hpp"
#include "core/Server.hpp"
#include "cgi/CgiHandler.hpp"
#include "utils/Logger.hpp"
#include "routing/LocationConfig.hpp"
#include "core/Config.hpp"
#include "routing/Router.hpp"
#include "network/Poller.hpp"
#include "http/Autoindex.hpp"
#include "filesystem/FileSystem.hpp"
#include "http/UploadHandler.hpp"
#include <sys/wait.h>
#include <fcntl.h>


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
        RequestParser parser(200);
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
    // COMPONENT: HTTP STATUS CODES
    // ==========================================
    else if (component == "http_status") {
        if (argc < 3) return 1;
        int code = std::atoi(argv[2]);
        
        std::cout << "Phrase: " << HttpStatus::reasonPhrase(code) << std::endl;
        std::cout << "Known: " << (HttpStatus::isKnown(code) ? "Yes" : "No") << std::endl;
        std::cout << "Error: " << (HttpStatus::isError(code) ? "Yes" : "No") << std::endl;
        return 0;
    }

    // ==========================================
    // COMPONENT: HTTP RESPONSE FORMATTING
    // ==========================================
    else if (component == "http_response") {
        // Create a mock response to test the toString() HTTP/1.1 formatting
        HttpResponse res(201, "Created user successfully");
        res.setHeader("Content-Type", "text/plain");
        res.setHeader("Content-Length", HttpResponse::numberToString(25));
        res.setConnectionClose(true);
        
        std::cout << res.toString();
        return 0;
    }

    // ==========================================
    // COMPONENT: HTTP ERROR PAGE
    // ==========================================
    else if (component == "http_error") {
        if (argc < 3) return 1;
        int code = std::atoi(argv[2]);
        
        HttpResponse res;
        if (ErrorPage::tryBuildDefault(code, res)) {
            std::cout << res.toString();
        } else {
            std::cout << "FAILED_ERROR_PAGE" << std::endl;
        }
        return 0;
    }

    // ==========================================
    // COMPONENT: SERVER CORE
    // ==========================================
    else if (component == "server") {
        if (argc < 3) return 1;
        std::string action = argv[2];
        
        if (action == "config") {
            Server server;
            // Test the config loader (argc=0, argv=NULL)
            if (server.loadConfig(0, NULL)) {
                std::cout << "SUCCESS_CONFIG" << std::endl;
            } else {
                std::cout << "FAILED_CONFIG" << std::endl;
            }
        }
        else if (action == "connections") {
            // We create a scope block to test the Server's destructor behavior
            {
                Server server;
                
                // Create dummy network connections
                Connection* c1 = new Connection(10);
                Connection* c2 = new Connection(20);
                
                // Map them inside the server
                server.addConnection(c1);
                server.addConnection(c2);
                
                // Manually cleanup the first connection
                server.cleanupConnection(10);
                
                std::cout << "SUCCESS_CONNECTIONS" << std::endl;
                
                // When 'server' goes out of scope here, its destructor should 
                // automatically delete 'c2' (FD 20) and prevent memory leaks.
            }
            std::cout << "SERVER_DESTROYED" << std::endl;
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

    // ==========================================
    // COMPONENT: CGI HANDLER
    // ==========================================
    else if (component == "cgi") {
        if (argc < 4) {
            std::cerr << "Usage: ./unit_tester cgi [script_path] [exec_path]" << std::endl;
            return 1;
        }
        std::string scriptPath = argv[2];
        std::string execPath = argv[3];

        // Mock parser payload
        RequestParser parser(200);
        std::string raw_request = "POST /dummy.py HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nHello";
        parser.feed(raw_request.c_str(), raw_request.length());

        CgiHandler cgi;
        if (!cgi.init(parser, scriptPath, execPath)) {
            std::cout << "FAILED_CGI_INIT" << std::endl;
            return 0;
        }

        // Simulate Poller Write
        std::string body = "Hello";
        write(cgi.getWriteFd(), body.c_str(), body.length());
        close(cgi.getWriteFd()); 
        int status;
        waitpid(cgi.getPid(), &status, 0);
        int flags = fcntl(cgi.getReadFd(), F_GETFL, 0);
        if (flags != -1) {
            fcntl(cgi.getReadFd(), F_SETFL, flags & ~O_NONBLOCK);
        }

        // Simulate Poller Read
        char buffer[1024];
        std::string output = "";
        int bytesRead;
        while ((bytesRead = read(cgi.getReadFd(), buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            output += buffer;
        }
        close(cgi.getReadFd());

        // Print machine-readable results for Python
        std::cout << "SUCCESS_CGI" << std::endl;
        std::cout << output; // Python will read this and assert its contents

        return 0;
    }

    // ==========================================
    // COMPONENT: ROUTER
    // ==========================================
    else if (component == "router") {
        if (argc < 3) return 1;
        std::string raw_payload = argv[2];

        // 1. Setup Router and mock configurations
        Router router;
        
        // This is a repository fixture, not a machine-specific /var path.
        LocationConfig imagesLoc("/images", "../www/data");
        imagesLoc.addAllowedMethod("GET");
        imagesLoc.addAllowedMethod("DELETE");
        router.addLocation(imagesLoc);

        LocationConfig apiLoc("/upload", "/var/www/uploads");
        apiLoc.addAllowedMethod("POST");
        router.addLocation(apiLoc);

        // 2. Parse the incoming fake request
        RequestParser parser(200);
        parser.feed(raw_payload.c_str(), raw_payload.length());

        // 3. Route the request and capture the response
        HttpResponse res;
        router.route(parser, res);

        // 4. Print output for Python to assert
        std::cout << "SUCCESS_ROUTER" << std::endl;
        std::cout << "STATUS: " << res.getStatusCode() << std::endl;
        return 0;
    }

// ==========================================
    // COMPONENT: CONFIG PARSER
    // ==========================================
    else if (component == "config_parser") {
        if (argc < 3) {
            std::cerr << "Usage: ./unit_tester config_parser [path_to_conf_file]" << std::endl;
            return 1;
        }
        std::string config_file_path = argv[2];
        
        Config config;
        // Test parsing the file
        if (config.loadConfig(config_file_path)) {
            std::cout << "SUCCESS_CONFIG_PARSER" << std::endl;
            const std::vector<ServerConfig>& servers = config.getServers();
            
            for (size_t i = 0; i < servers.size(); ++i) {
                std::cout << "Server Port: " << servers[i].port << std::endl;
                std::cout << "Server Name: " << servers[i].serverName << std::endl;
                std::cout << "Max Body Size: " << servers[i].clientMaxBodySize << std::endl;
                
                for (size_t j = 0; j < servers[i].locations.size(); ++j) {
                    std::cout << "Loc Path: " << servers[i].locations[j].getPath() << std::endl;
                    std::cout << "Loc Root: " << servers[i].locations[j].getRoot() << std::endl;
                    std::cout << "Autoindex: " << (servers[i].locations[j].getAutoindex() ? "on" : "off") << std::endl;
                    std::cout << "Upload: " << (servers[i].locations[j].isUploadEnabled() ? "on" : "off") << std::endl;
                    std::cout << "Upload Store: " << servers[i].locations[j].getUploadStore() << std::endl;
                    std::cout << "Method GET: " << (servers[i].locations[j].isMethodAllowed("GET") ? "yes" : "no") << std::endl;
                    std::cout << "Method POST: " << (servers[i].locations[j].isMethodAllowed("POST") ? "yes" : "no") << std::endl;
                }
            }
        } else {
            std::cout << "FAILED_CONFIG_PARSER" << std::endl;
        }
    }
    // ==========================================
    // COMPONENT: AUTOINDEX
    // ==========================================
    else if (component == "autoindex") {
        if (argc < 4) {
            std::cerr << "Usage: ./unit_tester autoindex [dir_path] [req_uri]" << std::endl;
            return 1;
        }
        std::string dirPath = argv[2];
        std::string reqUri = argv[3];
        HttpResponse res;
        
        if (Autoindex::tryBuildResponse(dirPath, reqUri, res)) {
            std::cout << "SUCCESS_AUTOINDEX\n";
            std::cout << res.toString();
        } else {
            std::cout << "FAILED_AUTOINDEX\n";
        }
        return 0;
    }
    std::cerr << "Unknown component: " << component << std::endl;
    return 1;
}
