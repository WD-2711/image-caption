#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#include <sys/types.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <unistd.h>
#include <functional>
#include <thread>
#include <random>
#include <map>
#include <cerrno>
#include <chrono>
#include <cstring>

#include "wepoll/wepoll.h"
#include "http_message.h"
#include "uri.h"

namespace http_server {
    const size_t kMaxBufferSize = 0x7fffffff;

    struct EventData {
        EventData() : fd(0), length(0), cursor(0), buffer() {}
        SOCKET fd;
        size_t length;
        size_t cursor;
        char buffer[kMaxBufferSize];
    };

    // Argument is HttpRequest and return is HttpResponse
    using HttpRequestHandler_t = std::function<HttpResponse(const HttpRequest&)>;

    LPCWSTR ConvertToLPCWSTR(const char* str)
    {
        int size = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
        std::wstring wstr(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str, -1, &wstr[0], size);
        return wstr.c_str();
    }

    // The server consists of:
    // - 1 main thread
    // - 1 listener thread that is responsible for accepting new connections
    // - Possibly many threads that process HTTP messages and communicate with clients via socket
    class HttpServer {
        public:
            explicit HttpServer(const std::string& host, std::uint16_t port) : host_(host),
                                                                               port_(port),
                                                                               sock_fd_(0),
                                                                               running_(false),
                                                                               worker_epoll_fd_(),
                                                                               rng_(std::chrono::steady_clock::now().time_since_epoch().count()),
                                                                               sleep_times_(10, 100) { CreateSocket(); }
            ~HttpServer() = default;
            HttpServer(HttpServer&&) = default;
            HttpServer& operator=(HttpServer&&) = default;

            void Start() {
                int opt = 1;
                sockaddr_in server_address;

                // Enable address and port reuse
                if (setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR) {
                    int error_code = WSAGetLastError();
                    std::cout << error_code << std::endl;
                    throw std::runtime_error("Failed to set socket options");
                }

                // IPV4 addr
                server_address.sin_family = AF_INET;
                // Accept any IP's connect
                server_address.sin_addr.s_addr = INADDR_ANY;
                // Save host address to server_address.sin_addr.s_addr
                InetPton(AF_INET, host_.c_str(), &(server_address.sin_addr.s_addr));
                // Save port address to server_address.sin_port
                server_address.sin_port = htons(port_);

                // Bind sock_fd_ to host_:port_
                if (bind(sock_fd_, (sockaddr *)&server_address, sizeof(server_address)) < 0) {
                    throw std::runtime_error("Failed to bind to socket");
                }

                // Listen connect request
                if (listen(sock_fd_, kBacklogSize) < 0) {
                    std::ostringstream msg;
                    msg << "Failed to listen on port " << port_;
                    throw std::runtime_error(msg.str());
                }

                SetUpEpoll();
                running_ = true;

                // Create thread to run Listen function
                listener_thread_ = std::thread(&Listen, this);

                // Create thread to run ProcessEvents function
                for (int i = 0; i < kThreadPoolSize; i++) {
                    worker_threads_[i] = std::thread(&ProcessEvents, this, i);
                }
            }

            void Stop() {
                running_ = false;
                for (int i = 0; i < kThreadPoolSize; i++) {
                    worker_threads_[i].join();
                }

                listener_thread_.join();
                closesocket(sock_fd_);
                for (int i = 0; i < kThreadPoolSize; i++) {
                    CloseHandle(worker_epoll_fd_[i]);
                }
            }

            void RegisterHttpRequestHandler(const std::string& path, HttpMethod method, const HttpRequestHandler_t callback) {
                Uri uri(path);
                request_handlers_[uri].insert(std::make_pair(method, std::move(callback)));
            }
            
            std::string host() const { return host_; }
            std::uint16_t port() const { return port_; }
            bool running() const { return running_; }              

        private:
            static constexpr int kBacklogSize = 1000;
            static constexpr int kMaxConnections = 10000;
            static constexpr int kMaxEvents = 10000;
            static constexpr int kThreadPoolSize = 5;

            std::string host_;
            std::uint16_t port_;
            SOCKET sock_fd_;
            bool running_;
            std::thread listener_thread_;
            std::thread worker_threads_[kThreadPoolSize];
            HANDLE worker_epoll_fd_[kThreadPoolSize];
            epoll_event worker_events_[kThreadPoolSize][kMaxEvents];
            std::map<Uri, std::map<HttpMethod, HttpRequestHandler_t>> request_handlers_;
            std::mt19937 rng_;
            std::uniform_int_distribution<int> sleep_times_;

            void CreateSocket() {
                // Init Winsock
                WSADATA wsaData;
                int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
                if (result != 0) {
                    throw std::runtime_error("WSAStartup failed with error.");
                }

                if ((sock_fd_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                    throw std::runtime_error("Failed to create a TCP socket");
                }

                // Set socket to Non-Blocking
                unsigned long nonBlocking = 1;
                if (ioctlsocket(sock_fd_, FIONBIO, &nonBlocking) != 0) {
                    throw std::runtime_error("Failed to set non-Blocking mode");
                }   
            }

            void SetUpEpoll() {
                for (int i = 0; i < kThreadPoolSize; i++) {
                    if ((worker_epoll_fd_[i] = epoll_create1(0)) < 0) {
                        throw std::runtime_error("Failed to create epoll file descriptor for worker");
                    }
                }
            }

            void Listen() {
                EventData *client_data;
                sockaddr_in client_address;
                socklen_t client_len = sizeof(client_address);
                SOCKET client_fd;
                int current_worker = 0;
                bool active = true;

                // Accept new connections and distribute tasks to worker threads
                while (running_) {
                    if (!active) {
                        std::this_thread::sleep_for(std::chrono::microseconds(sleep_times_(rng_)));
                    }
                    
                    // Accept new connections and create non-block socket
                    client_fd = accept(sock_fd_, (sockaddr *)&client_address, &client_len);
                    if (client_fd < 0 || client_fd == 0xffffffffffffffff) {
                        active = false;
                        continue;
                    }

                    active = true;
                    client_data = new EventData();
                    client_data->fd = client_fd;
                    
                    // Add client_fd to epoll instance
                    control_epoll_event(worker_epoll_fd_[current_worker], EPOLL_CTL_ADD, client_fd, EPOLLIN, client_data);
                    current_worker++;
                    if (current_worker == kThreadPoolSize) 
                        current_worker = 0;
                }
            }

            void ProcessEvents(int worker_id) {
                EventData *data;
                HANDLE epoll_fd = worker_epoll_fd_[worker_id];
                bool active = true;

                while (running_) {
                    if (!active) {
                        std::this_thread::sleep_for(std::chrono::microseconds(sleep_times_(rng_)));
                    }

                    // Wait epoll_fd send events
                    int nfds = epoll_wait(epoll_fd, worker_events_[worker_id], kMaxEvents, 0);
                    if (nfds <= 0) {
                        active = false;
                        continue;
                    }

                    active = true;
                    for (int i = 0; i < nfds; i++) {
                        const epoll_event &current_event = worker_events_[worker_id][i];
                        data = reinterpret_cast<EventData *>(current_event.data.ptr);

                        if ((current_event.events & EPOLLHUP) || (current_event.events & EPOLLERR)) {
                            // If event is pending or error
                            control_epoll_event(epoll_fd, EPOLL_CTL_DEL, data->fd);
                            closesocket(data->fd);
                            delete data;
                        } else if ((current_event.events == EPOLLIN) || (current_event.events == EPOLLOUT)) {
                            // If it is a read/write event
                            HandleEpollEvent(epoll_fd, data, current_event.events);
                        } else {  
                            // Something unexpected
                            control_epoll_event(epoll_fd, EPOLL_CTL_DEL, data->fd);
                            closesocket(data->fd);
                            delete data;
                        }
                    }
                }
            }
            
            void HandleEpollEvent(HANDLE epoll_fd, EventData* data, std::uint32_t events) {
                SOCKET fd = data->fd;
                EventData *request, *response;

                if (events == EPOLLIN) {
                    // Read from socket
                    request = data;
                    ssize_t byte_count = recv(fd, request->buffer, kMaxBufferSize, 0);
                    if (byte_count > 0) {
                        response = new EventData();
                        response->fd = fd;
                        // Process request and return response
                        HandleHttpData(*request, response);
                        control_epoll_event(epoll_fd, EPOLL_CTL_MOD, fd, EPOLLOUT, response);
                        delete request;
                    } else if (byte_count == 0) {  
                        // Client has closed connection
                        control_epoll_event(epoll_fd, EPOLL_CTL_DEL, fd);
                        closesocket(fd);
                        delete request;
                    } else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {  
                            // Resouce temporily can't use OR operation will be blocked
                            // We need retry
                            control_epoll_event(epoll_fd, EPOLL_CTL_MOD, fd, EPOLLIN, request);
                        } else {  
                            // Other error
                            control_epoll_event(epoll_fd, EPOLL_CTL_DEL, fd);
                            closesocket(fd);
                            delete request;
                        }
                    }
                } else {
                    // Write to socket
                    response = data;
                    ssize_t byte_count = send(fd, response->buffer + response->cursor, response->length, 0);
                    if (byte_count >= 0) {
                        if (byte_count < response->length) {  
                            // There are still bytes to write
                            response->cursor += byte_count;
                            response->length -= byte_count;
                            control_epoll_event(epoll_fd, EPOLL_CTL_MOD, fd, EPOLLOUT, response);
                        } else {  
                            // We have written the complete message, then change to receive mode
                            request = new EventData();
                            request->fd = fd;
                            control_epoll_event(epoll_fd, EPOLL_CTL_MOD, fd, EPOLLIN, request);
                            delete response;
                        }
                    } else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {  
                            // Retry
                            control_epoll_event(epoll_fd, EPOLL_CTL_ADD, fd, EPOLLOUT, response);
                        } else { 
                            // Other error
                            control_epoll_event(epoll_fd, EPOLL_CTL_DEL, fd);
                            closesocket(fd);
                            delete response;
                        }
                    }
                }
            }

            void HandleHttpData(const EventData& raw_request, EventData* raw_response) {
                std::string request_string(raw_request.buffer), response_string;
                HttpRequest http_request;
                HttpResponse http_response;

                try {
                    http_request = string_to_request(request_string);
                    http_response = HandleHttpRequest(http_request);
                } catch (const std::invalid_argument &e) {
                    http_response = HttpResponse(HttpStatusCode::BadRequest);
                    http_response.SetContent("Bad Request.");
                } catch (const std::logic_error &e) {
                    http_response = HttpResponse(HttpStatusCode::HttpVersionNotSupported);
                    http_response.SetContent("Http Version Not Supported.");
                } catch (const std::exception &e) {
                    http_response = HttpResponse(HttpStatusCode::InternalServerError);
                    http_response.SetContent("Internal Server Error.");
                }

                // Set response to write to client
                response_string = to_string(http_response, http_request.method() != HttpMethod::HEAD);
                int response_len = std::strlen(response_string.c_str()) > kMaxBufferSize ? kMaxBufferSize : std::strlen(response_string.c_str());
                memcpy(raw_response->buffer, response_string.c_str(), response_len);
                raw_response->length = response_string.length();
                
                if (to_string(http_request.method()) == "POST") {
                    // Print info
                    std::cout << "[+] URI: " << http_request.uri().path() << std::endl;
                    std::cout << "[+] Method: " << to_string(http_request.method()) << std::endl;
                    std::cout << "[+] Request content: "<< std::endl;
                    std::cout << http_request.content().substr(0, 100) << std::endl;
                    std::cout << "[+] Response content: "<< std::endl;
                    std::cout << http_response.content() << std::endl;
                    std::cout << std::endl;
                }
            }

            HttpResponse HandleHttpRequest(const HttpRequest& request) {
                auto it = request_handlers_.find(request.uri());
                if (it == request_handlers_.end()) {  
                    // This uri is not registered
                    return HttpResponse(HttpStatusCode::NotFound);
                }
                auto callback_it = it->second.find(request.method());
                if (callback_it == it->second.end()) {  
                    // No handler for this method
                    return HttpResponse(HttpStatusCode::MethodNotAllowed);
                }
                // Call handler to process the request (HttpRequestHandler_t)
                return callback_it->second(request);
            }
            
            void control_epoll_event(HANDLE epoll_fd, int op, SOCKET fd, std::uint32_t events = 0, void* data = nullptr) {
                if (op == EPOLL_CTL_DEL) {
                    if (epoll_ctl(epoll_fd, op, fd, nullptr) < 0) {
                        throw std::runtime_error("Failed to remove file descriptor");
                    }
                } else {
                    epoll_event ev;
                    ev.events = events;
                    ev.data.ptr = data;
                    if (epoll_ctl(epoll_fd, op, fd, &ev) < 0) {
                        throw std::runtime_error("Failed to add file descriptor");
                    }
                }
            }

    };
}

#endif