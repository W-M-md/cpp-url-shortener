# cpp-url-shortener

> 基于 Linux epoll + 单 Reactor 多线程模型的高性能 C++ HTTP 短链服务

![C++](https://img.shields.io/badge/C++-11-blue)
![Platform](https://img.shields.io/badge/Platform-Linux-green)
![License](https://img.shields.io/badge/License-MIT-yellow)

## 📖 项目简介

本项目是一个基于 Linux 的高并发短链接生成与跳转服务，支持 HTTP GET/POST 请求解析、MySQL 持久化存储、自研线程池异步处理、spdlog 日志系统与静态资源服务。

核心目标：解决传统阻塞 I/O 服务器在高并发场景下的性能瓶颈，通过 epoll + 非阻塞 I/O + 线程池实现高吞吐、低延迟。

## 📈 性能压测

使用 `wrk -t 2 -c 10 -d 10s http://127.0.0.1:8888/` 压测：

\`\`\`text
Requests/sec:   4622.08
Latency:        1.54ms
46282 requests in 10.01s
Socket errors:  0
\`\`\`

| 指标 | 数值 |
| :--- | :--- |
| **QPS** | **4622** |
| **平均延迟** | 1.54ms |
| **总请求数** | 46282 |
| **错误率** | 0% |
