#include "parser/RequestParser.hpp"
#include <algorithm>
#include <string>

static bool isRecognizedMethod(const std::string& method)
{
	static const char* const knownMethods[] = {
		"GET",
		"POST",
		"DELETE",
		"HEAD",
		"PUT",
		"PATCH",
		"OPTIONS",
		"TRACE",
		"CONNECT"
	};

	for (size_t i = 0; i < sizeof(knownMethods) / sizeof(knownMethods[0]); ++i)
	{
		if (method == knownMethods[i])
			return true;
	}
	return false;
}

static bool isImplementedMethod(const std::string& method)
{
	return method == "GET" || method == "POST" || method == "DELETE";
}

RequestParser::RequestParser(size_t maxBodySize) 
	: _state(STATE_REQUEST_LINE), _bodyType(BODY_NONE),
	_methodState(METHOD_UNKNOWN), _method(), _path(), _query(), _version(), _headers(), _body(),
	_headerBuffer(), _contentLength(0), _bytesRead(0), _headerBytes(0),
	_chunkState(CHUNK_SIZE), _currentChunkSize(0), _chunkHexBuffer(),
	_maxBodySize(maxBodySize), _oversizedBodyDrained(false)
{
}

char RequestParser::toLowerAscii(char c)
{
	if (c >= 'A' && c <= 'Z')
		return static_cast<char>(c - 'A' + 'a');
	return c;
}

std::string RequestParser::normalizeHeaderName(const std::string &value)
{
	std::string result(value);
	for (size_t i = 0; i < result.size(); ++i)
		result[i] = toLowerAscii(result[i]);
	return result;
}

bool RequestParser::isTokenChar(char c)
{
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
		|| (c >= '0' && c <= '9'))
		return true;
	return c == '!' || c == '#' || c == '$' || c == '%' || c == '&'
		|| c == '\'' || c == '*' || c == '+' || c == '-' || c == '.'
		|| c == '^' || c == '_' || c == '`' || c == '|' || c == '~';
}

bool RequestParser::parseDecimalSize(const std::string &value, size_t &result)
{
	result = 0;
	if (value.empty())
		return false;
	for (size_t i = 0; i < value.size(); ++i)
	{
		if (value[i] < '0' || value[i] > '9'
			|| result > (static_cast<size_t>(-1) - static_cast<size_t>(value[i] - '0')) / 10)
			return false;
		result = result * 10 + static_cast<size_t>(value[i] - '0');
	}
	return true;
}

bool RequestParser::parseHexSize(const std::string &value, size_t &result)
{
	result = 0;
	if (value.empty())
		return false;
	for (size_t i = 0; i < value.size(); ++i)
	{
		size_t digit;
		if (value[i] >= '0' && value[i] <= '9') digit = value[i] - '0';
		else if (value[i] >= 'a' && value[i] <= 'f') digit = value[i] - 'a' + 10;
		else if (value[i] >= 'A' && value[i] <= 'F') digit = value[i] - 'A' + 10;
		else return false;
		if (result > (static_cast<size_t>(-1) - digit) / 16)
			return false;
		result = result * 16 + digit;
	}
	return true;
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

	if(_version != "HTTP/1.1" && _version != "HTTP/1.0")
		return false;

	if(_method.empty() || _path.empty() || _path[0] != '/')
		return false;

	const std::string::size_type queryPos = _path.find('?');
	if (queryPos != std::string::npos)
	{
		_query = _path.substr(queryPos + 1);
		_path.erase(queryPos);
	}

	if (isImplementedMethod(_method))
		_methodState = METHOD_IMPLEMENTED;
	else if (isRecognizedMethod(_method))
		_methodState = METHOD_UNIMPLEMENTED;
	else
		_methodState = METHOD_UNKNOWN;

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
	{
		return false;
 	}

	for (size_t i = 0; i < key.size(); ++i)
		if (!isTokenChar(key[i]))
			return false;

	key = normalizeHeaderName(key);
	if ((key == "host" || key == "content-length" || key == "transfer-encoding")
		&& _headers.find(key) != _headers.end())
		return false;
	if (key == "content-length" && !parseDecimalSize(value, _contentLength))
		return false;

	_headers[key] = value;
	return true;
}

const std::string &RequestParser::getMethod() const
{
	return _method;
}

RequestParser::MethodState RequestParser::getMethodState() const
{
	return _methodState;
}

const std::string &RequestParser::getPath() const
{
	return _path;
}

const std::string &RequestParser::getQuery() const
{
	return _query;
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

bool RequestParser::isOversizedBodyDrained() const
{
	return _oversizedBodyDrained;
}

RequestParser::ParserState RequestParser::getState() const
{
	return _state;
}

RequestParser::ParseState RequestParser::getParseState() const
{
	if (_state == STATE_COMPLETE)
		return PARSE_SUCCESS;
	if (_state == STATE_ERROR || (_state == STATE_PAYLOAD_TOO_LARGE && _oversizedBodyDrained))
		return PARSE_ERROR;
	return PARSE_INCOMPLETE;
}

void RequestParser::determineBodyType()
{
	const bool hasLength = _headers.find("content-length") != _headers.end();
	const bool hasTransfer = _headers.find("transfer-encoding") != _headers.end();
	if (hasLength && hasTransfer)
	{
		_state = STATE_ERROR;
		return;
	}
	std::map<std::string, std::string>::const_iterator it = _headers.find("transfer-encoding");
	if (hasTransfer)
	{
		std::string transferEncoding = normalizeHeaderName(trim(it->second));
		if (transferEncoding != "chunked")
		{
			_state = STATE_ERROR;
			return;
		}
		_bodyType = BODY_CHUNKED;
		return;
	}
	//If not chunked, check for Content-Length
	it = _headers.find("content-length");
	if(hasLength)
	{
		_bodyType = BODY_CONTENT_LENGTH;
		if (_maxBodySize != 0 && _contentLength > _maxBodySize)
		{
			_state = STATE_PAYLOAD_TOO_LARGE;
			_bytesRead = 0;
			_oversizedBodyDrained = (_contentLength == 0);
			return;
		}
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
	if (_state == STATE_PAYLOAD_TOO_LARGE)
	{
		drainOversizedBody(data, length);
		return;
	}

	size_t i = 0;
	while(i < length && _state != STATE_BODY && _state != STATE_ERROR && _state != STATE_COMPLETE && _state != STATE_PAYLOAD_TOO_LARGE)
	{
		if (++_headerBytes > 32768)
		{
			_state = STATE_ERROR;
			return;
		}
		_headerBuffer += data[i];
		if(_headerBuffer.size() >= 2 && _headerBuffer.substr(_headerBuffer.size() - 2) == "\r\n")
		{
			std::string line = _headerBuffer.substr(0, _headerBuffer.size() - 2);
			_headerBuffer.clear();
			if (_state == STATE_REQUEST_LINE && line.size() > 8192)
			{
				_state = STATE_ERROR;
				return;
			}

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
	else if (_state == STATE_PAYLOAD_TOO_LARGE && i < length)
	{
		drainOversizedBody(data + i, length - i);
	}
}

void RequestParser::drainOversizedBody(const char* data, size_t length)
{
	(void)data;
	if (_bodyType != BODY_CONTENT_LENGTH || _oversizedBodyDrained)
		return;
	const size_t remaining = _contentLength - _bytesRead;
	const size_t consumed = std::min(length, remaining);
	_bytesRead += consumed;
	if (_bytesRead == _contentLength)
		_oversizedBodyDrained = true;
}

void RequestParser::processBody(const char* data, size_t length)
{
	if(_bodyType == BODY_CONTENT_LENGTH)
	{
		size_t bytesToRead = std::min(length, _contentLength - _bytesRead);
		if (_maxBodySize != 0 && _body.size() + bytesToRead > _maxBodySize)
		{
			_state = STATE_PAYLOAD_TOO_LARGE;
			return;
		}
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
		while(i < length && _state != STATE_ERROR && _state != STATE_COMPLETE && _state != STATE_PAYLOAD_TOO_LARGE)
		{
			if(_chunkState == CHUNK_SIZE)
			{
				char c = data[i++];
				_chunkHexBuffer += c;
				if(_chunkHexBuffer.size() >= 2 && _chunkHexBuffer.substr(_chunkHexBuffer.size() - 2) == "\r\n")
				{
					std::string size_line = _chunkHexBuffer.substr(0, _chunkHexBuffer.size() - 2);
					const std::string::size_type extensionPos = size_line.find(';');
					if (extensionPos != std::string::npos)
						size_line.erase(extensionPos);
					if (!parseHexSize(size_line, _currentChunkSize))
					{
						_state = STATE_ERROR;
					}
					else
					{
						if (_maxBodySize != 0 && _body.size() + _currentChunkSize > _maxBodySize)
						{
							_state = STATE_PAYLOAD_TOO_LARGE;
							return;
						}
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
				if (_maxBodySize != 0 && _body.size() + 1 > _maxBodySize)
				{
					_state = STATE_PAYLOAD_TOO_LARGE;
					return;
				}
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
