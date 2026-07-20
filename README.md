
# Project: webserv

## Team

- Bartek
- Tomek
- Przemek

## Description

## Goals

## Worksplit

We've identified the following blocks and assigned as follows:

- Socket Engine (Bartek)
- Event Loop (Bartek)
- HTTP Parser (Tomek)
- CGI Logic (Tomek)
- Static files & HTTP responses  (Bartek)
- Uploads (Przemek)
- Directory listing (Przemek)
- Default error pages (Przemek)
- Routing & Methods (Tomek)
- Config-dependent behavior (Tomek)
- Stability & testing (Bartek, Przemek, Tomek)
- Docs (Bartek, Przemek, Tomek)

## Tech details
### Directory structure and function mapping
#### Networking (Bartek)
- `network/` ---> Socket Engine, Event Loop, Non‑blocking, IO Multiplexing
#### Parsing (Tomek)
- `parser/` ---> Parser, Request Line, Header Map, Body Handling
#### HTTP (Bartek)
- `http/` ---> HTTP response builder, Status codes
#### CGI (Tomek)
- `cgi/` ---> Pipe Setup, Fork & Exec, Env Variables, CGI Logic
#### Filesystem (Przemek)
- `filesystem/` ---> Uploads, Directory listing, Static file serving
#### Routing (Tomek)
- `routing/` ---> Routing & methods, Config‑dependent behavior
#### Utils
- `utils/` ---> Additional helper files
#### WWW
- `www/` ---> HTTP Server contents

## Classes and interfaces
### 1. HTTP Parser (`parser/RequestParser`)

#### What it does
Since TCP doesn't guarantee we receive a full HTTP request in one go, the `RequestParser` is a state machine. It takes incoming raw bytes from a socket (which might be fragmented) and incrementally builds a structured `HttpRequest` object. It handles the Request-Line, HTTP Headers, and the Body (including `Transfer-Encoding: chunked`).

#### Key Interfaces
*   `void feedData(const std::string& raw_data);`
    Feed newly read bytes into the parser. It won't block; it just advances the internal state machine.
*   `bool isComplete() const;`
    Returns `true` if the entire request (including the body) has been successfully parsed.
*   `bool hasError() const;`
    Checks if a bad request (e.g., malformed syntax, 400 Bad Request) was encountered.
*   `HttpRequest getRequest() const;`
    Retrieves the parsed request object.

#### How to use it
```cpp
RequestParser parser;

// Inside your socket read event:
char buffer[8192];
ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
if (bytes_read > 0) {
    parser.feedData(std::string(buffer, bytes_read));
    
    if (parser.isComplete()) {
        HttpRequest req = parser.getRequest();
        // Pass 'req' to the Router!
    } else if (parser.hasError()) {
        // Send 400 Bad Request
    }
}
```

### 2. CGI Logic

CGI execution is tricky because we have to fork the process, map headers to environment variables, and use pipes for communication, all without blocking [cite: 2]. 

#### `cgi/CgiEnv.hpp`
**What it does:** Builds the `char** envp` array required by `execve`. It translates standard HTTP headers (like `Content-Length`) and Server contexts into CGI-standard environment variables (like `CONTENT_LENGTH`, `REQUEST_METHOD`, `PATH_INFO`).
**Key Interfaces:**
*   `CgiEnv(const HttpRequest& req, const LocationConfig& loc, const ServerConfig& srv);`
*   `char** getEnv() const;`

#### `cgi/CgiProcess.hpp`
**What it does:** Wraps the low-level Unix syscalls (`pipe()`, `fork()`, `execve()`, `dup2()`). It sets up non-blocking pipes so our `EventLoop` can write the request body to the CGI script and read the CGI output asynchronously.
**Key Interfaces:**
*   `void execute(const std::string& script_path, char** envp);`
*   `int getReadFd() const;` and `int getWriteFd() const;` (To register with `poll()`).
*   `bool isFinished();` (Checks `waitpid` with `WNOHANG`).

#### `cgi/CgiHandler.hpp`
**What it does:** The high-level orchestrator. It uses `CgiEnv` to prep the data and `CgiProcess` to run it. It bridges the gap between the `Router` and the low-level process execution.
**How to use it:**
```cpp
CgiHandler cgi(request, location_config);
cgi.start();
// Register cgi.getReadFd() in your epoll/poll loop to read the script's output!
```

### 3. Routing & Methods

#### `routing/LocationConfig.hpp`
**What it does:** A simple data container representing a single `location {}` block from our `.conf` file [cite: 2]. 
**Key Interfaces:**
*   `bool isMethodAllowed(const std::string& method) const;`
*   `std::string getRoot() const;`
*   `bool isAutoindexEnabled() const;`
*   `std::string getUploadPath() const;`

#### `routing/Router.hpp`
**What it does:** The brain of the request-response cycle. It takes an `HttpRequest` and the `Config`, finds the most specific matching `LocationConfig`, checks method permissions (GET, POST, DELETE), and decides what to do (serve static file, generate autoindex, or pass to CGI).
**Key Interfaces:**
*   `HttpResponse route(const HttpRequest& request, const ServerConfig& config);`

**How to use it:**
```cpp
Router router;
// After parsing is complete:
HttpResponse response = router.route(parsed_request, current_server_config);
// Serialize 'response' and write to the client socket
```

### 4. Core & Utils

#### `core/Config.hpp`
**What it does:** Parses the main configuration file at startup. It holds multiple `ServerConfig` blocks, mapping hostnames and ports to their respective behaviors [cite: 2].
**Key Interfaces:**
*   `void load(const std::string& filepath);`
*   `std::vector<ServerConfig> getServers() const;`

#### `utils/Logger.hpp`
**What it does:** A static utility for standardized console output.
**Key Interfaces:**
*   `Logger::info(const std::string& msg);`
*   `Logger::error(const std::string& msg);`
*   `Logger::debug(const std::string& msg);`

**How to use it:**
```cpp
Logger::info("New client connected on fd " + StringUtils::to_string(client_fd));
```

### 5. Networking (`network/`)

#### `network/SocketEngine.hpp`
**What it does:** Acts as the foundational TCP server setup. It handles the low-level socket creation, binding, listening, and sets the sockets to non-blocking mode (`O_NONBLOCK`) to prevent the server from hanging.
**Key Interfaces:**
*   `void init();`
    Initializes the socket, binds it to the specified port, and starts listening for incoming connections.
*   `Connection* acceptConnection(size_t maxBodySize);`
    Accepts a new client connection, sets its file descriptor to non-blocking, and wraps it in a `Connection` object.
*   `int getFd() const;`
    Returns the server's listening socket file descriptor.

#### `network/EventLoop.hpp`
**What it does:** The beating heart of the server. It multiplexes all I/O operations using `poll()`. It monitors all server sockets for new clients and all client sockets for incoming requests or readiness to send responses. It also includes a "sweeper" mechanism to aggressively close timed-out or broken connections.
**Key Interfaces:**
*   `EventLoop(const std::vector<SocketEngine*>& engines, const std::vector<ServerConfig>& servers);`
    Initializes the loop with the active server engines and their configurations.
*   `void run();`
    Starts the infinite loop, calling `poll()`, processing `POLLIN` (read) and `POLLOUT` (write) events, and managing the `Connection` lifecycles.

**How to use it:**
```cpp
// Inside Server::run()
std::vector<SocketEngine*> engines = setupEngines();
EventLoop loop(engines, config.getServers());
loop.run(); // Blocks until server shutdown

#### `network/Connection.hpp`
What it does: Represents a single, persistent client connection. It tracks the clients file descriptor, the dedicated RequestParser to specific client, the response buffer waiting to be sent, and the timestamp of their last activity (for timeout management).
**Key Interfaces:**
*   RequestParser& getParser();
*   void appendResponse(const std::string& data);
*   void reset();
    Clears the parser and buffer to handle Keep-Alive connections[cite: 2].
*   bool isTimedOut(int timeout_seconds) const;

## AI usage

## TODO

[ ] all
