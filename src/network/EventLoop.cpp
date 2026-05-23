#include "network/EventLoop.hpp"
#include "parser/RequestParser.hpp"
#include <iostream>

EventLoop::EventLoop()
{
}

void EventLoop::run()
{
	std::cout << "Event loop started" << std::endl;

	while(1)
	{
		// placeholder
	}
}
