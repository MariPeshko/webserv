#ifndef REQUEST_HPP
# define REQUEST_HPP

# include <string>
# include <map>
# include <vector>
# include <iostream>
# include <sstream>

// Data object that holds parsed request
class	Request {
	public:
		Request();
		~Request();

		enum MethodType {
			GET,
			POST,
			DELETE,
			INVALID
		};

		// setters
		void	setRequestLineFormatValid(bool valid);
		void	setMethod(const std::string &method);
		void	setUri(const std::string &uri);
		void	setVersion(const std::string &version);
		void	addHeader(const std::string &key, const std::string &value);
		void	setBody(const std::string &body);

		// getters
		bool				getRequestLineFormatValid() const;
		std::string			getMethod() const;
		std::string			getUri() const;
		std::string			getVersion() const;
		std::map<std::string, std::string>	getHeaders() const;
		std::string			getBody() const;

	private:
		bool						_validFormatReqLine;
		MethodType					_method;
		std::string					_uri;
		std::string					_httpVersion;
		std::map<std::string, std::string>	_headers;
		std::string					_body;
};

#endif