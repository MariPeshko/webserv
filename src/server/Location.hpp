#ifndef LOCATION_HPP
# define LOCATION_HPP

# include "../../inc/Webserv.hpp"

class Location {
	public:
		Location();
		~Location();
		
		// Setters
		void	setPath(const std::string& path);
		void	setRoot(const std::string& root);
		void	setIndex(const std::string& index);
		void	setAutoindex(bool autoindex);
		void	addAllowedMethod(const std::string& method);
		void	setReturn(int code, const std::string& url);
		void	addCgi(const std::string& ext, const std::string& path);
		void	addLocation(const Location& location);
		void	setAlias(const std::string& alias);
		void	setClientMaxBodySize(const std::string& size);

		// Getters
		const std::vector<std::string>&				getAllowedMethods() const;
		const std::map<std::string, std::string>&	getCgi() const;
		const std::vector<Location>&				getLocations() const;
		const std::string&	getPath() const;
		const std::string&	getRoot() const;
		const std::string&	getAlias() const;
		const std::string&	getIndex() const;
		bool				getAutoindex() const;
		int					getReturnCode() const;
		const std::string&	getReturnUrl() const;
		const std::string&	getClientMaxBodySize() const;
		
		void print() const;

		std::string					_path;

	private:
		std::string					_root;
		std::string					_alias;
		std::vector<std::string>	_allowed_methods;
		std::string					_index;
		bool						_autoindex;
		int							_return_code;
		std::string					_return_url;
		std::map<std::string, std::string>	_cgi;
		std::vector<Location>		_locations;
		std::string					_client_max_body_size;
};

#endif

