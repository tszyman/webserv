#include "parser/RequestParser.hpp"
#include <string>

RequestParser::RequestParser() 
	: _state(STATE_REQUEST_LINE), _bodyType(BODY_NONE), 
	_contentLength(0), _bytesRead(0),
	_chunkState(CHUNK_SIZE), _currentChunkSize(0)
{
}
std::string RequestParser::trim(const std::string &str)
{
	std::string::size_type start = 0;
	while(start < str.size() && (str[start] == ' ' || str[start] == '\t'))
		start++;

	std::string::size_type end = str.size();
	while(end > start && (str[end-1] == ' ' || str[end - 1] == '\t'))
		end--;

	return str.substr(start, end - start);
}

bool RequestParser::parseRequestLine(const std::string &line)
{
	std::string::size_type pos1 = line.find(' ');
	if(pos1 == std::string::npos)
		return false;

	std::string::size_type pos2 = line.find(' ', pos1 + 1);
	if(pos2 == std::string::npos)
		return false;

	_method = line.substr(0, pos1);
	_path = line.substr(pos1 + 1, pos2 - pos1 - 1);
	_version = line.substr(pos2 + 1);

	if(_method != "GET" && _method != "POST" && _method != "DELETE")
		return false;

	if(_version != "HTTP/1.1" && _version != "HTTP/1.0")
		return false;

	if(_path.empty() || _path[0] != '/')
		return false;

	return true;
}

bool RequestParser::parseHeaderLine(const std::string &line)
{
	if(line.empty()){
		return true;
	}

	std::string::size_type pos = line.find(':');
	if(pos == std::string::npos)
		return false;

	// if(line.find(':', pos + 1) != std::string::npos)
	// 	return false;

	std::string key = trim(line.substr(0, pos));
	std::string value = trim(line.substr(pos + 1));

	if(key.empty())
		return false;

	size_t i = 0;
	while(i < key.size())
	{
		if(key[i] == ' ' || key[i] == '\t')
			return false;
		i++;
	}

	_headers[key] = value;
	return true;
}

const std::string &RequestParser::getMethod() const
{
	return _method;
}

const std::string &RequestParser::getPath() const
{
	return _path;
}

const std::string &RequestParser::getVersion() const
{
	return _version;
}

const std::map<std::string, std::string> &RequestParser::getHeaders() const
{
	return _headers;
}

const std::vector<char> &RequestParser::getBody() const
{
	return _body;
}

RequestParser::ParserState RequestParser::getState() const
{
	return _state;
}

void RequestParser::determineBodyType()
{
	std::map<std::string, std::string>::const_iterator it = _headers.find("Transfer-Encoding");
	if(it != _headers.end() && it->second.find("chunked") != std::string::npos)
	{
		_bodyType = BODY_CHUNKED;
		return;
	}
	//If not chunked, check for Content-Length
	it = _headers.find("Content-Length");
	if(it != _headers.end())
	{
		_bodyType = BODY_CONTENT_LENGTH;
		_contentLength = static_cast<size_t>(std::strtol(it->second.c_str(),NULL,10));
		_body.reserve(_contentLength);
		if(_contentLength == 0)
		{
			_state = STATE_COMPLETE;
		}
		return;
	}

	//If neither is present, there is no body
	_bodyType = BODY_NONE;
	_state = STATE_COMPLETE;
}

void RequestParser::feed(const char* data, size_t length)
{
	if (_state == STATE_ERROR || _state == STATE_COMPLETE)
		return;

	size_t i = 0;
	while(i < length && _state != STATE_BODY && _state != STATE_ERROR && _state != STATE_COMPLETE)
	{
		_headerBuffer += data[i];
		if(_headerBuffer.size() >= 2 && _headerBuffer.substr(_headerBuffer.size() - 2) == "\r\n")
		{
			std::string line = _headerBuffer.substr(0, _headerBuffer.size() - 2);
			_headerBuffer.clear();

			if(line.empty())
			{
				_state = STATE_BODY;
				determineBodyType();
			} else if(_state == STATE_REQUEST_LINE)
			{
				if(!parseRequestLine(line))
					_state = STATE_ERROR;
				else
					_state = STATE_HEADERS;
			} else if(_state == STATE_HEADERS)
			{
				if(!parseHeaderLine(line))
					_state = STATE_ERROR;
			}
		}
		i++;
	}
	// Process the reminder of the buffer as body data
	if(_state == STATE_BODY && i < length)
	{
		processBody(data + i, length - i);
	}
}

void RequestParser::processBody(const char* data, size_t length)
{
	if(_bodyType == BODY_CONTENT_LENGTH)
	{
		size_t bytesToRead = std::min(length, _contentLength - _bytesRead);
		_body.insert(_body.end(), data, data + bytesToRead);
		_bytesRead += bytesToRead;
		if(_bytesRead >= _contentLength)
		{
			_state = STATE_COMPLETE;
		}
	} 
	else if(_bodyType == BODY_CHUNKED)
	{
		size_t i = 0;
		while(i < length && _state != STATE_ERROR && _state != STATE_COMPLETE)
		{
			if(_chunkState == CHUNK_SIZE)
			{
				char c = data[i++];
				_chunkHexBuffer += c;
				if(_chunkHexBuffer.size() >= 2 && _chunkHexBuffer.substr(_chunkHexBuffer.size() - 2) == "\r\n")
				{
					std::string size_line = _chunkHexBuffer.substr(0, _chunkHexBuffer.size() - 2);
					char* endptr;
					long parsed_size = std::strtol(size_line.c_str(), &endptr, 16);
					// Validation checks:
					// 1. endptr == size_line.c_str() means no digits were parsed at all
					// 2. *endptr != '\0' means there were training invalid chars (like 'Z')
					// 3. parsed_size < 0 protects against overflow or negative values
					if (endptr == size_line.c_str() || *endptr != '\0' || parsed_size < 0)
					{
						_state = STATE_ERROR;
					}
					else
					{
						_currentChunkSize = static_cast<size_t>(std::strtol(size_line.c_str(), NULL, 16));
						_chunkHexBuffer.clear();
						_bytesRead = 0;
						if(_currentChunkSize == 0)
							_state = STATE_COMPLETE;
						else
							_chunkState = CHUNK_DATA;
					}
				}
			}
			else if(_chunkState == CHUNK_DATA)
			{
				_body.push_back(data[i++]);
				_bytesRead++;
				
				if(_bytesRead >= _currentChunkSize)
				{
					_chunkState = CHUNK_CRLF;
					_chunkHexBuffer.clear();
				} 
			}
			else if(_chunkState == CHUNK_CRLF)
			{
				_chunkHexBuffer += data[i++];
				if(_chunkHexBuffer.size() == 2)
				{
					if(_chunkHexBuffer == "\r\n")
					{
						_chunkState = CHUNK_SIZE;
						_chunkHexBuffer.clear();
					}
					else
					{
						_state = STATE_ERROR;
					}
				}
			}
		}
	}
}