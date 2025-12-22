#include "Location.hpp"

Location::Location() : _autoindex(false), _return_code(0) {}
Location::~Location() {}

void Location::setPath(const std::string& path) { _path = path; }
void Location::setRoot(const std::string& root) { _root = root; }
void Location::setIndex(const std::string& index) { _index = index; }
void Location::setAutoindex(bool autoindex) { _autoindex = autoindex; }
void Location::addAllowedMethod(const std::string& method) { _allowed_methods.push_back(method); }
void Location::setReturn(int code, const std::string& url) { _return_code = code; _return_url = url; }
void Location::addCgi(const std::string& ext, const std::string& path) { _cgi[ext] = path; }
void Location::addLocation(const Location& location) { _locations.push_back(location); }
void Location::setAlias(const std::string& alias) { _alias = alias; }
void Location::setClientMaxBodySize(const std::string& size) { _client_max_body_size = size; }

const std::string& Location::getPath() const { return _path; }
const std::string& Location::getRoot() const { return _root; }
const std::string& Location::getAlias() const { return _alias; }
const std::vector<std::string>& Location::getAllowedMethods() const { return _allowed_methods; }
const std::string& Location::getIndex() const { return _index; }
bool Location::getAutoindex() const { return _autoindex; }
int Location::getReturnCode() const { return _return_code; }
const std::string& Location::getReturnUrl() const { return _return_url; }
const std::map<std::string, std::string>& Location::getCgi() const { return _cgi; }
const std::vector<Location>& Location::getLocations() const { return _locations; }
const std::string& Location::getClientMaxBodySize() const { return _client_max_body_size; }

void Location::print() const {
    std::cout << "    Location: " << _path << std::endl;
    if (!_root.empty()) std::cout << "      root: " << _root << std::endl;
    if (!_alias.empty()) std::cout << "      alias: " << _alias << std::endl;
    if (!_allowed_methods.empty()) {
        std::cout << "      methods: ";
        for (size_t i = 0; i < _allowed_methods.size(); i++) {
            std::cout << _allowed_methods[i] << (i < _allowed_methods.size() - 1 ? ", " : "");
        }
        std::cout << std::endl;
    }
    if (!_index.empty()) std::cout << "      index: " << _index << std::endl;
    if (!_client_max_body_size.empty()) std::cout << "      client_max_body_size: " << _client_max_body_size << std::endl;
    std::cout << "      autoindex: " << (_autoindex ? "on" : "off") << std::endl;
    if (_return_code != 0) {
        std::cout << "      return: " << _return_code << " " << _return_url << std::endl;
    }
    if (!_cgi.empty()) {
        std::cout << "      cgi:" << std::endl;
        for (std::map<std::string, std::string>::const_iterator it = _cgi.begin(); it != _cgi.end(); ++it) {
            std::cout << "        " << it->first << " -> " << it->second << std::endl;
        }
    }
    if (!_locations.empty()) {
        std::cout << "      Nested Locations:" << std::endl;
        for (size_t i = 0; i < _locations.size(); ++i) {
            _locations[i].print();
        }
    }
}

