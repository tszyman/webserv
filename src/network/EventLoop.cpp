#include "network/EventLoop.hpp"
#include "parser/RequestParser.hpp"
#include <iostream>

EventLoop::EventLoop()
{   
}

void EventLoop::run()
{
    RequestParser parser;
    bool ok;

    std::cout << "Event loop started" << std::endl;
    
    // TEST parseRequestLine
    ok = parser.parseRequestLine("GET /index.html HTTP/1.1");
    std::cout << "parseRequestLine result: " << ok << std::endl;

    while(1)
    {
        // placeholder
    }
}
