#include "network/EventLoop.hpp"
#include "parser/RequestParser.hpp"
#include <iostream>

EventLoop::EventLoop()
{
}

void EventLoop::run()
{
	std::cout << "Event loop started" << std::endl;

	// TESTY
	RequestParser parser;
	bool ok;

	ok = parser.parseRequestLine("GET /index.html HTTP/1.1");
	std::cout << "Parsed: " << ok << std::endl;
	ok = parser.parseHeaderLine("Host: localhost");
	std::cout << "Parsed: " << ok << std::endl;
	ok = parser.parseHeaderLine("Content-Length: 42");
	std::cout << "Parsed: " << ok << std::endl;
	ok = parser.parseHeaderLine("");
	std::cout << "Parsed: " << ok << std::endl;

	std::cout << "Headers parsed: " << parser.getHeaders().size() << std::endl;

	while (1)
	{
		// placeholder
	}
}
