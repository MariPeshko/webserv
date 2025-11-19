#ifndef REQUEST_HPP
# define REQUEST_HPP
# include <string>
# include <map>
# include <vector>

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

        void            setMethod(const std::string &method);
        void            setUri(const std::string &uri);
        void            setVersion(const std::string &version);
        void            addHeader(const std::string &key, const std::string &value);
        void            setBody(const std::string &body);

        std::string     getMethod() const;
        std::string     getUri() const;
        std::string     getVersion() const;
        std::map<std::string, std::string> getHeaders() const;
        std::string     getBody() const;
        void            setParsingState(ParsingState state);
        void            parseRequest(char* raw_request, size_t length); 
       
    private:
        MethodType     _method;
        std::string     _uri;
        std::string     _version;
        std::map<std::string, std::string> _headers;
        std::string     _body;
        ParsingState   _parsing_state;
};

#endif