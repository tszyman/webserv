#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP
#include <string>
#include <map>
#include <vector>
#include <cstdlib>


class RequestParser
{
	public:
		enum ParserState{
			STATE_REQUEST_LINE,
			STATE_HEADERS,
			STATE_BODY,
			STATE_COMPLETE,
			STATE_PAYLOAD_TOO_LARGE,
			STATE_ERROR
		};
		
		RequestParser(size_t maxBodySize);
		
		void feed(const char* data, size_t length);				//feed raw data from the socket into the parser
		
		ParserState getState() const;
		const std::string &getMethod() const;
		const std::string &getPath() const;
		const std::string &getVersion() const;
		const std::map<std::string, std::string> &getHeaders() const;
		const std::vector<char> &getBody() const;
		bool isOversizedBodyDrained() const;
		
	private:
		enum BodyType{
			BODY_NONE,
			BODY_CONTENT_LENGTH,
			BODY_CHUNKED
		};

		ParserState _state;
		BodyType _bodyType;

		std::string _method;
		std::string _path;
		std::string _version;
		std::map<std::string, std::string> _headers;
		std::vector<char> _body;

		std::string _headerBuffer;
		size_t _contentLength;
		size_t _bytesRead;
		
		enum ChunkState{
			CHUNK_SIZE,
			CHUNK_DATA,
			CHUNK_CRLF
		};
		ChunkState _chunkState;
		size_t _currentChunkSize;
		std::string _chunkHexBuffer;
		size_t _maxBodySize;
		bool _oversizedBodyDrained;

		void determineBodyType();
		void processBody(const char* data, size_t length);
		void drainOversizedBody(const char* data, size_t length);

		std::string trim(const std::string &str);
		bool parseRequestLine(const std::string &line);
		bool parseHeaderLine(const std::string &line);
};

#endif
