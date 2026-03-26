#pragma once

constexpr char SERVER_IP[] = "0.0.0.0";
constexpr USHORT SERVER_PORT = 6000;
constexpr USHORT PACKET_HEADER_SIZE = sizeof(USHORT);

constexpr int WORKER_THREAD_CREATE_COUNT = 2;
constexpr int WORKER_THREAD_MAX_COUNT = 2;
constexpr int MAX_SESSION_COUNT = 1000;

constexpr int SESSION_RECV_BUFFER_SIZE = 8192;
constexpr int SESSION_SEND_BUFFER_SIZE = 8192;
constexpr int SERIALIZED_BUFFER_SIZE = 4096;

constexpr unsigned int SESSION_POOL_SIZE = 1024;
constexpr unsigned int SERIALIZED_BUFFER_POOL_SIZE = 2048;
