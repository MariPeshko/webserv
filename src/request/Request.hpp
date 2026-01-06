#ifndef REQUEST_HPP
# define REQUEST_HPP

# include <string>
# include <map>
# include <vector>
# include <iostream>
# include <sstream>

class	PrintUtils;

// Data object that holds parsed request
class	Request {
	friend class PrintUtils;

	public:
		Request();
		~Request();

		enum MethodType {
			GET,
			POST,
			DELETE,
			HEAD,
			INVALID
		};

		// setters
		void	setRequestLineFormatValid(bool value);
		void	setHeadersFormatValid(bool value);
		void	setHost(const std::string &host);
		void	setMethod(const std::string &method);
		void	setUri(const std::string &uri);
		void	setVersion(const std::string &version);
		void	addHeader(const std::string &key, const std::string &value);
		void	ifConnNotPresent();
		void	setBody(const std::string &body);
		void	setChunked(bool value);
		void	setStatusCode(short code);

		// getters
		bool				getRequestLineFormatValid() const;
		bool				getHeadersFormatValid() const;
		bool				isContentLengthHeader() const;
		bool				isTransferEncodingHeader() const;
		std::string			getMethod() const;
		MethodType			getEnumMethod() const { return _method;}
		std::string&		getUri();
		const std::string&	getUri() const;
		std::string			getVersion() const;
		std::string&		getBody();
		const std::string&	getBody() const;
		std::map<std::string, std::string>	getHeaders() const;
		const std::string &	getHeaderValue(const std::string header_name) const;
		const std::string &	getHost() const;
		short				getStatusCode() const;

	private:
		bool						_validFormatReqLine;
		bool						_validFormatHeaders;
		MethodType					_method;
		std::string					_uri;
		std::string					_httpVersion;
		std::map<std::string, std::string>	_headers;
		std::string					_body;
		bool						_bodyChunked;
		std::string					_host;
		short						_statusCode;
};

#endif
