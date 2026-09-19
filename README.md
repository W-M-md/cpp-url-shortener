# cpp-url-shortener

> 基于 Linux epoll + 单 Reactor 多线程模型的高性能 C++ HTTP 短链服务

![C++](https://img.shields.io/badge/C++-11-blue)
![Platform](https://img.shields.io/badge/Platform-Linux-green)
![License](https://img.shields.io/badge/License-MIT-yellow)

## 📖 项目简介

本项目是一个基于 Linux 的高并发短链接生成与跳转服务，支持 HTTP GET/POST 请求解析、MySQL 持久化存储、自研线程池异步处理、spdlog 日志系统与静态资源服务。

核心目标：解决传统阻塞 I/O 服务器在高并发场景下的性能瓶颈，通过 epoll + 非阻塞 I/O + 线程池实现高吞吐、低延迟。

## 📈 性能压测

使用 `wrk -t 2 -c 10 -d 10s http://127.0.0.1:8888/` 压测，结果如下：

```text
Running 10s test @ http://127.0.0.1:8888/
  2 threads and 10 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     6.01ms   14.13ms 210.45ms   95.29%
    Req/Sec   789.56    346.05     1.68k    66.16%
  15723 requests in 10.08s, 1.78MB read
  Socket errors: connect 0, read 124, write 0, timeout 0
Requests/sec:   1559.44
Transfer/sec:    181.22KB
