#ifndef HTTPPARSER_HPP
# define HTTPPARSER_HPP

# include <string>
# include <map>
# include <vector>
# include <iostream>
# include <sstream>

// Data object that holds parsed request
class	HttpParser {
	public:
		HttpParser();
		~HttpParser();

	static const size_t MAX_REQUEST_LINE  = 8192;      // 8 KB
    static const size_t MAX_HEADER_BLOCK  = 16384;     // 16 KB
    static const size_t MAX_BUFFER_TOTAL  = 1024*1024; // 1 MB fallback

	private:
};

#endif