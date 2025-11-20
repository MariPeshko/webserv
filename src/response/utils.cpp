#include "Response.hpp"

std::string generateStatusMessage(short status_code) {
    switch (status_code) {
        case 200:
            return "OK";
        case 300:
            return "Multiple Choices";
        case 400:
            return "Bad Request";
        case 500:
            return "Internal Server Error";
        default:
            return "Unknown Error";
    }
}