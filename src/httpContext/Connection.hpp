#ifndef CONNECTION_HPP
# define CONNECTION_HPP

# include "../../inc/Webserv.hpp"
# include <sys/socket.h>
# include <netinet/in.h>

#define CON_DEBUG 0
#define YELLOW "\033[33m"
#define RESET "\033[0m"

/**
 * Briefly: transport + buffer
 * 
 * Data object that holds socket fd, client address,
 * input buffer, calling recv().
 */
class	Connection {
	public:
		Connection();
		~Connection();

		// setters
		void	setFd(int fd);
		void	setClientAddress(const sockaddr_in& client_address);

		// getters
		int					getFd() const;
		std::string &		getBuffer();
		const sockaddr_in&	getClientAddress() const;

		ssize_t			receiveData();
		
		void			updateLastActivity();
		bool			hasTimedOut(time_t timeout) const;

	private:
		int 				_fd;
		struct sockaddr_in	_client_address;
		std::string			_request_buffer;
		time_t				_last_activity;
};

#endif