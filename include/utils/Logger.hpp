#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>

class Logger {
	public:
		// Level definitions
		enum Level{
			DEBUG,
			INFO,
			WARNING,
			ERROR
		};
		// Main logging methods
		static void debug(std::string& message);
		static void info(std::string& message);
		static void warning(std::string& message);
		static void error(std::string& message);

		static void setLogLevel(Level level); // to hide DEBUG messages in production
	private:
		Logger();                            // private constructor so noone can instantiate Logger log;
		static Level _currentLevel;
		static std::string getTimeStamp();
};

#endif // LOGGER_HPP