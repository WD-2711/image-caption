#ifndef HTTP_MESSAGE_H_
#define HTTP_MESSAGE_H_

#include <map>
#include <string>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <sstream>

#include "uri.h"

namespace http_server {

    enum class HttpMethod {
        GET,
        HEAD,
        POST,
        PUT,
        DELETE,
        CONNECT,
        OPTIONS,
        TRACE,
        PATCH
    };

    enum class HttpVersion {
        HTTP_0_9 = 9,
        HTTP_1_0 = 10,
        HTTP_1_1 = 11,
        HTTP_2_0 = 20
    };

    enum class HttpStatusCode {
        Continue = 100,
        SwitchingProtocols = 101,
        EarlyHints = 103,
        Ok = 200,
        Created = 201,
        Accepted = 202,
        NonAuthoritativeInformation = 203,
        NoContent = 204,
        ResetContent = 205,
        PartialContent = 206,
        MultipleChoices = 300,
        MovedPermanently = 301,
        Found = 302,
        NotModified = 304,
        BadRequest = 400,
        Unauthorized = 401,
        Forbidden = 403,
        NotFound = 404,
        MethodNotAllowed = 405,
        RequestTimeout = 408,
        ImATeapot = 418,
        InternalServerError = 500,
        NotImplemented = 501,
        BadGateway = 502,
        ServiceUnvailable = 503,
        GatewayTimeout = 504,
        HttpVersionNotSupported = 505
    };

    std::string to_string(HttpMethod method) {
        switch (method) {
            case HttpMethod::GET:
                return "GET";
            case HttpMethod::HEAD:
                return "HEAD";
            case HttpMethod::POST:
                return "POST";
            case HttpMethod::PUT:
                return "PUT";
            case HttpMethod::DELETE:
                return "DELETE";
            case HttpMethod::CONNECT:
                return "CONNECT";
            case HttpMethod::OPTIONS:
                return "OPTIONS";
            case HttpMethod::TRACE:
                return "TRACE";
            case HttpMethod::PATCH:
                return "PATCH";
            default:
                return std::string();
        }
    }

    std::string to_string(HttpVersion version) {
        switch (version) {
            case HttpVersion::HTTP_0_9:
                return "HTTP/0.9";
            case HttpVersion::HTTP_1_0:
                return "HTTP/1.0";
            case HttpVersion::HTTP_1_1:
                return "HTTP/1.1";
            case HttpVersion::HTTP_2_0:
                return "HTTP/2.0";
            default:
                return std::string();
        }
    }

    std::string to_string(HttpStatusCode status_code) {
        switch (status_code) {
            case HttpStatusCode::Continue:
                return "Continue";
            case HttpStatusCode::Ok:
                return "OK";
            case HttpStatusCode::Accepted:
                return "Accepted";
            case HttpStatusCode::MovedPermanently:
                return "Moved Permanently";
            case HttpStatusCode::Found:
                return "Found";
            case HttpStatusCode::BadRequest:
                return "Bad Request";
            case HttpStatusCode::Forbidden:
                return "Forbidden";
            case HttpStatusCode::NotFound:
                return "Not Found";
            case HttpStatusCode::MethodNotAllowed:
                return "Method Not Allowed";
            case HttpStatusCode::ImATeapot:
                return "I'm a Teapot";
            case HttpStatusCode::InternalServerError:
                return "Internal Server Error";
            case HttpStatusCode::NotImplemented:
                return "Not Implemented";
            case HttpStatusCode::BadGateway:
                return "Bad Gateway";
            default:
                return std::string();
        }
    }

    HttpMethod string_to_method(const std::string& method_string) {
        std::string method_string_uppercase;

        std::transform(method_string.begin(), method_string.end(),
                       std::back_inserter(method_string_uppercase),
                       [](char c) { return toupper(c); });

        if (method_string_uppercase == "GET") {
            return HttpMethod::GET;
        } else if (method_string_uppercase == "HEAD") {
            return HttpMethod::HEAD;
        } else if (method_string_uppercase == "POST") {
            return HttpMethod::POST;
        } else if (method_string_uppercase == "PUT") {
            return HttpMethod::PUT;
        } else if (method_string_uppercase == "DELETE") {
            return HttpMethod::DELETE;
        } else if (method_string_uppercase == "CONNECT") {
            return HttpMethod::CONNECT;
        } else if (method_string_uppercase == "OPTIONS") {
            return HttpMethod::OPTIONS;
        } else if (method_string_uppercase == "TRACE") {
            return HttpMethod::TRACE;
        } else if (method_string_uppercase == "PATCH") {
            return HttpMethod::PATCH;
        } else {
            throw std::invalid_argument("Unexpected HTTP method");
        }
    }

    HttpVersion string_to_version(const std::string& version_string) {
        std::string version_string_uppercase;

        std::transform(version_string.begin(), version_string.end(),
                       std::back_inserter(version_string_uppercase),
                       [](char c) { return toupper(c); });

        if (version_string_uppercase == "HTTP/0.9") {
            return HttpVersion::HTTP_0_9;
        } else if (version_string_uppercase == "HTTP/1.0") {
            return HttpVersion::HTTP_1_0;
        } else if (version_string_uppercase == "HTTP/1.1") {
            return HttpVersion::HTTP_1_1;
        } else if (version_string_uppercase == "HTTP/2" || version_string_uppercase == "HTTP/2.0") {
            return HttpVersion::HTTP_2_0;
        } else {
            throw std::invalid_argument("Unexpected HTTP version");
        }
    }
    
    class HttpMessageInterface {
        public:
            HttpMessageInterface() : version_(HttpVersion::HTTP_1_1) {}
            virtual ~HttpMessageInterface() = default;

            void SetHeader(const std::string& key, const std::string& value) {
                headers_[key] = std::move(value);
            }

            void RemoveHeader(const std::string& key) {
                headers_.erase(key);
            }

            void SetContent(const std::string& content) {
                content_ = std::move(content);
                SetHeader("Content-Length", std::to_string(content_.length()));
            }

            void ClearContent(const std::string& content) {
                content_.clear();
                SetHeader("Content-Length", std::to_string(content_.length()));
            }

            HttpVersion version() const {
                return version_;
            }

            std::string header(const std::string& key) const {
                if(headers_.count(key) > 0) 
                    return headers_.at(key);
                return std::string();
            }

            std::map<std::string, std::string> headers() const {
                return headers_;
            }

            std::string content() const { 
                return content_; 
            }

            size_t content_length() const { 
                return content_.length(); 
            }

        protected:
            HttpVersion version_;
            std::map<std::string, std::string> headers_;
            std::string content_;
    };

    class HttpRequest : public HttpMessageInterface {
        public:
            HttpRequest() : method_(HttpMethod::GET) {}
            ~HttpRequest() = default;

            void SetMethod(HttpMethod method) { 
                method_ = method; 
            }

            void SetUri(const Uri& uri) { 
                uri_ = std::move(uri); 
            }

            HttpMethod method() const { 
                return method_; 
            }

            Uri uri() const { 
                return uri_; 
            }

            friend std::string to_string(const HttpRequest& request);
            friend HttpRequest string_to_request(const std::string& request_string);
        
        private:
            HttpMethod method_;
            Uri uri_;
    };

    class HttpResponse : public HttpMessageInterface {
        public:
            HttpResponse() : status_code_(HttpStatusCode::Ok) {}
            HttpResponse(HttpStatusCode status_code) : status_code_(status_code) {}
            ~HttpResponse() = default;

            void SetStatusCode(HttpStatusCode status_code) { 
                status_code_ = status_code; 
            }

            HttpStatusCode status_code() const { 
                return status_code_; 
            }

            friend std::string to_string(const HttpResponse& request, bool send_content);
            friend HttpResponse string_to_response(const std::string& response_string);
        
        private:
            HttpStatusCode status_code_;
    };

    std::string to_string(const HttpRequest& request) {
        std::ostringstream oss;

        oss << to_string(request.method()) << ' ';
        oss << request.uri().path() << ' ';
        oss << to_string(request.version()) << "\r\n";
        for (const auto& p : request.headers())
            oss << p.first << ": " << p.second << "\r\n";
        
        oss << "\r\n";
        oss << request.content();
        return oss.str();
    }

    std::string to_string(const HttpResponse& response, bool send_content = true) {
        std::ostringstream oss;

        oss << to_string(response.version()) << ' ';
        oss << static_cast<int>(response.status_code()) << ' ';
        oss << to_string(response.status_code()) << "\r\n";
        for (const auto& p : response.headers())
            oss << p.first << ": " << p.second << "\r\n";
        
        oss << "\r\n";
        if (send_content) 
            oss << response.content();
        return oss.str();
    }

    HttpRequest string_to_request(const std::string& request_string) {
        std::string start_line, header_lines, message_body;
        std::istringstream iss;
        HttpRequest request;
        std::string line, method, path, version;  // used for first line
        std::string key, value;                   // used for header fields
        Uri uri;
        size_t lpos = 0, rpos = 0;

        rpos = request_string.find("\r\n", lpos);
        if (rpos == std::string::npos) {
            throw std::invalid_argument("Could not find request start line");
        }
        start_line = request_string.substr(lpos, rpos - lpos);

        lpos = rpos + 2;
        rpos = request_string.find("\r\n\r\n", lpos);
        if (rpos != std::string::npos) {
            // has header
            header_lines = request_string.substr(lpos, rpos - lpos);
        
            lpos = rpos + 4;
            rpos = request_string.length();
            if (lpos < rpos) {
                message_body = request_string.substr(lpos, rpos - lpos);
            }
        }

        // parse the start line
        iss.clear();
        iss.str(start_line);
        iss >> method >> path >> version;
        if (!iss.good() && !iss.eof()) {
            throw std::invalid_argument("Invalid start line format");
        }
        request.SetMethod(string_to_method(method));
        request.SetUri(Uri(path));
        if (string_to_version(version) != request.version()) {
            throw std::logic_error("HTTP version not supported");
        }

        // parse header fields
        iss.clear();
        iss.str(header_lines);
        while (std::getline(iss, line)) {
            std::istringstream header_stream(line);
            std::getline(header_stream, key, ':');
            std::getline(header_stream, value);

            // remove whitespaces from the two strings
            key.erase(std::remove_if(key.begin(), key.end(), [](char c) { return std::isspace(c); }), key.end());
            value.erase(std::remove_if(value.begin(), value.end(), [](char c) { return std::isspace(c); }), value.end());
            
            request.SetHeader(key, value);
        }

        request.SetContent(message_body);
        return request;
    }
    
    HttpResponse string_to_response(const std::string& response_string) {
        throw std::logic_error("Method not implemented");
    }

}

#endif