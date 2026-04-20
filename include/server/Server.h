/**
 * @file Server.h
 * @brief Класс для управления сетевыми подключениями.
 */
#ifndef SERVER_H
#define SERVER_H

#include <cstdint>
#include <string>

class Server {
private:
    uint16_t port;
    int serverSocket; // Файловый дескриптор главного сокета
    bool isRunning;
public:
    /**
     * @brief Конструктор сервера.
     * @param port Порт, который будет слушать сервер (по умолчанию 8080).
     */
    explicit Server(uint16_t port = 8080);
    ~Server();

    /**
     * @brief Запуск сервера (создание сокета, привязка и прослушивание).
     */
    void start();

    /**
     * @brief Остановка сервера.
     */
    void stop();
};

#endif