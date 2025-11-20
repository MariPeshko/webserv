#ifndef REQUEST_HPP
# define REQUEST_HPP

# include <string>
# include <map>
# include <vector>
#include <iostream>
#include <sstream>

class Request {
    public:
        Request();
        ~Request();

         enum ParsingState {
            REQUEST_LINE,
            HEADERS,
            BODY,
            COMPLETE,
            ERROR
        };

        enum MethodType {
            GET,
            POST,
            DELETE
        };

        // setters
        void            setMethod(const std::string &method);
        void            setUri(const std::string &uri);
        void            setVersion(const std::string &version);
        void            addHeader(const std::string &key, const std::string &value);
        void            setBody(const std::string &body);
        void            setParsingState(ParsingState state);
        
        // getters
        std::string     getMethod() const;
        std::string     getUri() const;
        std::string     getVersion() const;
        std::map<std::string, std::string> getHeaders() const;
        std::string     getBody() const;

        void            parseRequest(char* raw_request); 
        void            parseRequestLine(char* raw_request);

    private:
        MethodType     _method;
        std::string     _uri;
        std::string     _httpVersion;
        std::map<std::string, std::string> _headers;
        std::string     _body;
        ParsingState   _parsing_state;
};

#endif