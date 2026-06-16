#include "utils/Logger.hpp"
#include <iostream>
#include <ctime>

// Colours
#define RESET	"\033[0m"
#define RED		"\033[31m"
#define YELLOW	"\033[33m"
#define CYAN	"\033[36m"
#define GRAY	"\033[90m"

// Initialize static variable (default INFO level)
Logger::Level Logger::_currentLevel = Logger::INFO;

void Logger::setLogLevel(Level level){
	_currentLevel = level;
}

// Generates a timestamp in the format: YYYY-MM-DD HH:MM:SS
std::string Logger::getTimeStamp()
{
	time_t rawtime;
	struct tm * timeinfo;
	char buffer[80];

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	//Format time
	strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
	return std::string(buffer);
}

//debug and info write to std::cout, warn and error to std::cerr
void Logger::debug(const std::string& message)
{
	if(_currentLevel <= DEBUG){
		std::cout << GRAY << "[" << getTimeStamp() << "] [DEBUG]   " << message << RESET << std::endl;
	}
}
void Logger::info(const std::string& message)
{
	if(_currentLevel <= INFO){
		std::cout << CYAN << "[" << getTimeStamp() << "] [INFO]    " << message << RESET << std::endl;
	}
}
void Logger::warning(const std::string& message)
{
	if(_currentLevel <= WARNING){
		std::cerr << YELLOW << "[" << getTimeStamp() << "] [WARNING] " << message << RESET << std::endl;
	}
}
void Logger::error(const std::string& message)
{
	if(_currentLevel <= ERROR){
		std::cerr << RED << "[" << getTimeStamp() << "] [ERROR]   " << message << RESET << std::endl;
	}
}