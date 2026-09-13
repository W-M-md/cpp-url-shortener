#include <iostream>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <unordered_map>
#include <mysql/mysql.h>
#include <stdlib.h>
#include <vector>
#include <fstream>
#include <regex>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include "threadpool.h"

MYSQL* g_conn;
std::mutex mysql_mutex;

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void mod_epoll(int fd, int epfd, uint32_t events) {
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = events | EPOLLET;
    if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == -1) {
        spdlog::error("epoll_ctl mod fd");
    }
}

std::string escaped_json(const std::string &s)
{
    std::string out;
    for (char c : s)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n"; 
            break;
        case '\r':
            out += "\\r";
            break;
        default: 
            out += c;
            break;
        }
    }
    return out;
}

std::string build_response(const std::string& path, const std::string& request) {
    std::lock_guard<std::mutex> lock(mysql_mutex);
    if (path == "/shorten") {
        std::string long_url = "https://default.com";
        size_t pos = request.find("url=");
        
        if (pos != std::string::npos) {
            pos += 4;
            size_t end = request.find(" ", pos);
            if (end == std::string::npos) end = request.find("\r\n", pos);
            long_url = request.substr(pos, end - pos);
        }

        std::string short_code = "abc" + std::to_string(time(nullptr) % 10000);

        std::vector<char> escaped_url(long_url.length() * 2 + 1);
        mysql_real_escape_string(g_conn, escaped_url.data(), long_url.c_str(), long_url.length());

        std::string sql = "INSERT INTO url_map (short_code, long_url) VALUES('"
                            + short_code + "', '" + escaped_url.data() + "')";
        if (mysql_query(g_conn, sql.c_str()) != 0) {
            spdlog::error("插入失败：{}", mysql_error(g_conn));
            return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
        }


        std::string safe_long_url = escaped_json(long_url);

        std::string json_response = R"({
            "code": 0,
            "message": "success",
            "data": {
                "short_url": "https://reactive-aqueduct-trophy.ngrok-free.dev/)" + short_code + 
                R"(","long_url": ")" + safe_long_url + R"("
            }
        })";

        return "HTTP/1.1 200 OK\r\n"
                "Content-Length: " + std::to_string(json_response.size()) + "\r\n"
                "Content-Type: application/json; charset=utf-8\r\n"
                "Connection: close\r\n\r\n" + 
                json_response;
    }


    std::regex short_code_pattern("^/([a-zA-Z0-9]{4,8})$");
    std::smatch match;
    if (std::regex_match(path, match, short_code_pattern)) {
        std::string code = match[1];
        
        std::vector<char> escaped_code(code.size() * 2 + 1);
        mysql_real_escape_string(g_conn, escaped_code.data(), code.c_str(), code.length());
        std::string sql = "SELECT long_url FROM url_map WHERE short_code = '" + 
                        std::string(escaped_code.data()) + "'";
        if (mysql_query(g_conn, sql.c_str()) != 0) {
            spdlog::error("查询失败: {}", mysql_error(g_conn));
            return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
        }

        MYSQL_RES * res = mysql_store_result(g_conn);
        if (res == nullptr) {
            return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        std::string long_url;
        if (row != nullptr && row[0] != nullptr) {
            long_url = row[0];
        }

        mysql_free_result(res);

        if (long_url.empty()) {
            return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n";
        }

        return "HTTP/1.1 302 Found\r\n"
                "Location: " + long_url + "\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n\r\n";
    }

    std::string full_path = "./www" + path;
    if (path == "/") full_path = "./www/index.html";

    std::ifstream file(full_path, std::ios::binary);
    if (!file.is_open()) {
        return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Length: " + std::to_string(content.size()) + "\r\n";
    response += "Content-Type: text/html; charset=utf-8\r\n";
    response += "Connection: close\r\n\r\n";
    response += content;
    return response;
}

int main() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    //sink 接收器
    //stdout_color_sink_mt  打印到终端屏幕，带颜色
    //rotating_file_sink_mt 写入硬盘文件

    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/server.log", 1024 * 1024 * 5, 3);
    //滚动文件：写满就换到下一页
    
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("multi", spdlog::sinks_init_list{console_sink, file_sink}));
    //spdlog::logger    记录器
    //当调用 spding::info() 时消息会交给 logger ,然后分发给 console_sink, file_sink

    spdlog::set_level(spdlog::level::debug);
    //level 日志级别
    //只输出设置级别以上的输出

    spdlog::flush_on(spdlog::level::info);
    //程序通常将数据存储到缓冲区中，等达到一定数量在一起写入磁盘
    //这一句意思是将 info 以上的立即写进文硬盘

    const char* user = getenv("DB_USER") ? getenv("DB_USER") : "root";
    const char* pass = getenv("DB_PASS") ? getenv("DB_PASS") : "123456";

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) { spdlog::error("socket"); return 1; }
    
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) == -1) { spdlog::error("bind"); return 1; }

    if (listen(listen_fd, 5) == -1) { spdlog::error("listen"); return 1; }

    int epfd = epoll_create(1);
    if (epfd == -1) { spdlog::error("epoll_create"); return 1; }

    MYSQL* conn = mysql_init(nullptr);
    if (conn == nullptr) {
        spdlog::error("MySQL 初始化失败");
        return 1;
    }

    if (mysql_real_connect(conn, "127.0.0.1", user, pass, "data", 3306, nullptr, 0) == nullptr) {
        spdlog::error("连接失败：{}", mysql_error(conn));
        mysql_close(conn);
        return 1;
    }

    g_conn = conn;

    spdlog::info("HTTP 静态服务器启动，请访问 http://127.0.0.1:8888");

    set_nonblocking(listen_fd);

    epoll_event ev{};
    ev.data.fd = listen_fd;
    ev.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) { spdlog::error("epoll_ctl"); return 1; }

    const int MAX_EVENTS = 64;
    epoll_event events[MAX_EVENTS];

    std::unordered_map<int, std::string> pending_buf;
    std::unordered_map<int, std::string> recv_buffer;

    ThreadPool pool(4);
    spdlog::info("线程池已启动（4个工作线程）");

    while (true) {
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            if (errno == EINTR) {
                continue;
            }
            spdlog::error("epoll_wait");
            break;
        } 

        for (size_t i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            uint32_t ev_flags = events[i].events;

            if (ev_flags &(EPOLLHUP | EPOLLERR)) {
                spdlog::error("客户端异常，fd = {}", fd);
                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                close(fd);
                pending_buf.erase(fd);
                continue;
            }

            if (fd == listen_fd) {
                while (true) {
                    int client_fd = accept(fd, nullptr, nullptr);
                    if (client_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        spdlog::error("accept");
                        break;
                    }

                    ev.data.fd = client_fd;
                    ev.events = EPOLLIN | EPOLLET;
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                        spdlog::error("epoll_ctl add client_fd");
                        close(client_fd);
                    }
                }

                continue;
            }

            if (ev_flags & EPOLLOUT) {
                auto it = pending_buf.find(fd);
                if (it != pending_buf.end() && !it->second.empty()) {
                    std::string& data = it->second;
                    int sent = send(fd, data.data(), data.size(), 0);
                    if (sent == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                        spdlog::error("send");
                        close(fd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                        pending_buf.erase(fd);
                        recv_buffer.erase(fd);
                        continue;
                    }
                    data.erase(0, sent);
                    if (data.empty()) {
                        pending_buf.erase(fd);
                        recv_buffer.erase(fd);
                        close(fd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                    }
                } else {
                    mod_epoll(fd, epfd, EPOLLIN);
                }
                continue;
            }

            if (ev_flags & EPOLLIN) {
                char buffer[4096];
                while (true) {
                    int n = recv(fd, buffer, sizeof(buffer) - 1, 0);
                    if (n > 0) {
                        recv_buffer[fd].append(buffer, n);

                        std::string& request = recv_buffer[fd];
                        size_t hander_end = request.find("\r\n\r\n");
                        if (hander_end != std::string::npos) {
                            size_t Comtent_Length = 0;
                            size_t cl_pos = request.find("Content-Length: ");
                            if (cl_pos != std::string::npos) {
                                cl_pos += 16;
                                size_t cl_end = request.find("\r\n", cl_pos);
                                if (cl_end != std::string::npos) {
                                    std::string cl_str = request.substr(cl_pos, cl_end - cl_pos);
                                    try {
                                        Comtent_Length = std::stoul(cl_str);
                                    } catch (...) {
                                        close(fd);
                                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                                        break;
                                    }
                                }
                            }

                            size_t total_expected = hander_end + 4 + Comtent_Length;
                            if (request.size() >= total_expected) {
                                std::string complate_request = request.substr(0, total_expected);
                                std::string path = "/";

                                if (complate_request.find("GET ") == 0) {
                                    size_t start = 4;
                                    size_t end = complate_request.find(" ", start);
                                    if (end != std::string::npos) {
                                        std::string full_path = complate_request.substr(start, end - start);
                                        size_t qpos = full_path.find("?");
                                        
                                        if (qpos != std::string::npos) {
                                            path = full_path.substr(0, qpos);
                                        } else {
                                            path = full_path;
                                        }
                                    }
                                } else if (complate_request.find("POST ") == 0) {
                                    size_t start = 5;
                                    size_t end = complate_request.find(" ", start);
                                    
                                    if (end != std::string::npos) {
                                        path = complate_request.substr(start, end - start);
                                    }
                                }

                                if (path.find("..") != std::string::npos) path = "/";

                                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                                recv_buffer.erase(fd);
                                //调用业务逻辑
                                pool.Task([fd, path, complate_request](){
                                    std::string response = build_response(path, complate_request);

                                    int sent = 0;
                                    while (sent < response.length()) {
                                        int cnt = send(fd, response.c_str() + sent, response.length() - sent, 0);
                                        if (cnt == -1) {
                                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                                spdlog::warn("send EAGAIN, 关闭连接 fd = {}", fd);
                                                break;
                                            }
                                            spdlog::error("send 失败，{} (fd = {})",strerror(errno), fd);
                                            break;
                                        }

                                        if (cnt == 0) break;

                                        sent += cnt;
                                    }
                                    shutdown(fd, SHUT_WR);
                                    close(fd);
                                });
                            } else {
                                break;
                            }
                        }
                    } else if (n == 0) {
                        spdlog::info("客户端正常退出，fd = {}",fd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                        close(fd);
                        pending_buf.erase(fd);
                        recv_buffer.erase(fd);
                        break;
                    } else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        spdlog::error("recv");
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                        close(fd);
                        pending_buf.erase(fd);
                        recv_buffer.erase(fd);
                        break;
                    }
                }
            }
        }
    }

    close(listen_fd);
    close(epfd);
    return 0;
}