#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00

#include <string>
#include <chrono>
#include <thread>
#include <iostream>
#include <stdexcept>

#include "http_message.h"
#include "http_server.h"
#include "uri.h"
#include "image_handler.h"

using http_server::HttpMethod;
using http_server::HttpRequest;
using http_server::HttpResponse;
using http_server::HttpServer;
using http_server::HttpStatusCode;
using http_server::request_handler;

int main(void) {
    // Can receive connection from any IP
    std::string host = "0.0.0.0";
    int port = 8080;
    HttpServer server(host, port);

    // Register many handler functions
    auto send_html = [](const HttpRequest& request) -> HttpResponse {
        HttpResponse response(HttpStatusCode::Ok);
        std::string content;
        content += request_handler(request.content(), request.content_length());

        response.SetHeader("Content-Type", "text/plain");
        response.SetHeader("Access-Control-Allow-Origin", "http://localhost:3000");
        response.SetContent(content);
        return response;
    };

    server.RegisterHttpRequestHandler("/image-upload", HttpMethod::POST, send_html);

    try {
        std::cout << "Starting the web server.." << std::endl;
        server.Start();
        std::cout << "Server listening on " << host << ":" << port << std::endl;

        std::cout << "Enter [quit] to stop the server" << std::endl;
        std::string command;
        while (std::cin >> command, command != "quit") {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "'quit' command entered. Stopping the web server.." << std::endl;
        server.Stop();
        std::cout << "Server stopped" << std::endl;
    } catch (std::exception& e) {
        std::cerr << "An error occurred #1" << std::endl;
        return -1;
    }

    return 0;
}