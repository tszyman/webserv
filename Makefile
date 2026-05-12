# =========================
# Project
# =========================

NAME        = webserv
CXX         = c++
CXXFLAGS    = -Wall -Wextra -Werror -std=c++98
RM          = rm -f

# =========================
# Directories
# =========================

SRC_DIR     = src
INC_DIR     = include
OBJ_DIR     = obj

# =========================
# Source files
# =========================

SRC_FILES = \
    main.cpp \
    core/Server.cpp \
    core/Config.cpp \
    \
    network/SocketEngine.cpp \
    network/EventLoop.cpp \
    network/Connection.cpp \
    network/Poller.cpp \
    \
    http/HttpRequest.cpp \
    http/HttpResponse.cpp \
    http/StatusCodes.cpp \
    \
    parser/RequestParser.cpp \
    parser/ChunkedDecoder.cpp \
    \
    routing/Router.cpp \
    routing/LocationConfig.cpp \
    \
    cgi/CgiHandler.cpp \
    cgi/CgiProcess.cpp \
    cgi/CgiEnv.cpp \
    \
    filesystem/FileSystem.cpp \
    filesystem/DirectoryListing.cpp \
    filesystem/UploadHandler.cpp \
    \
    utils/Logger.cpp \
    utils/StringUtils.cpp \
    utils/Error.cpp

SRCS = $(addprefix $(SRC_DIR)/, $(SRC_FILES))
OBJS = $(addprefix $(OBJ_DIR)/, $(SRC_FILES:.cpp=.o))

# =========================
# Include paths
# =========================

INCLUDES = -I$(INC_DIR)

# =========================
# Rules
# =========================

all: $(NAME)

$(NAME): $(OBJS)
    $(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

# =========================
# Object rule
# =========================

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
    mkdir -p $(dir $@)
    $(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =========================
# Clean rules
# =========================

clean:
    $(RM) -r $(OBJ_DIR)

fclean: clean
    $(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re