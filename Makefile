NAME = webserv

# - Source files
SRCS =	src/main.cpp \
		src/server/Config.cpp \
		src/server/Server.cpp \
		src/server/ServerManager.cpp \
		src/httpContext/Connection.cpp \
		src/httpContext/HttpContext.cpp \
		src/httpContext/HttpParser.cpp \
		src/response/Response.cpp \
		src/response/utils.cpp \
		src/request/Request.cpp \
		src/cgi/CgiHandler.cpp  \
		src/logger/Logger.cpp \

# - Header files
HEADERS	= inc/Webserv.hpp
INCLUDE = -I inc

# - Object files
OBJ_DIR = build/
OBJS = $(addprefix $(OBJ_DIR), $(SRCS:.cpp=.o))

# - Compiler
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g3

LOG_FILE = webserv.log

RM = rm -rf

# - Colors
RESET = "\033[0m"
GREEN = "\033[1m\033[32m"
RED = "\033[1m\033[31m"

all: $(NAME)

$(NAME) : $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo $(GREEN)webserv compiled $(RESET)

$(OBJ_DIR)%.o: %.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

clean:
	$(RM) $(OBJ_DIR)
	$(RM) $(LOG_FILE)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re