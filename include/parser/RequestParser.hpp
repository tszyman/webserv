#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP
#include <string>
#include <map>
#include <vector>
#include <cstdlib>


class RequestParser
{
	public:
		enum ParseState {
			PARSE_INCOMPLETE,
			PARSE_SUCCESS,
			PARSE_ERROR
		};

		enum MethodState {
			METHOD_UNKNOWN,
			METHOD_UNIMPLEMENTED,
			METHOD_IMPLEMENTED
		};

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
		
		ParseState getParseState() const;
		ParserState getState() const;
		const std::string &getMethod() const;
		MethodState getMethodState() const;
		const std::string &getPath() const;
		const std::string &getQuery() const;
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
		MethodState _methodState;

		std::string _method;
		std::string _path;
		std::string _query;
		std::string _version;
		std::map<std::string, std::string> _headers;
		std::vector<char> _body;

		std::string _headerBuffer;
		size_t _contentLength;
		size_t _bytesRead;
		size_t _headerBytes;
		
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
		static char toLowerAscii(char c);
		static std::string normalizeHeaderName(const std::string &value);
		static bool isTokenChar(char c);
		static bool parseDecimalSize(const std::string &value, size_t &result);
		static bool parseHexSize(const std::string &value, size_t &result);
};

#endif
