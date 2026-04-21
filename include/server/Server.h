/**
 * @file Server.h
 * @brief Класс для управления сетевыми подключениями.
 */
#ifndef SERVER_H
#define SERVER_H

#include <thread>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <mutex>

class Server {
private:
    uint16_t port;
    int serverSocket; // Файловый дескриптор главного сокета
    bool isRunning;
    
    // Таблица маршрутизации
    // Хранит соответствие: ID авторизованного пользователя -> дескриптор его активного сокета.
    // Это позволяет серверу пересылать сообщения нужным клиентам в реальном времени.
    std::unordered_map<int, int> activeClients; 
    std::mutex clientsMutex;                   

    /**
     * @brief Метод для обслуживания конкретного клиента.
     * Запускается в отдельном потоке.
     * @param clientSocket Сокет подключенного клиента.
     */
    void handleClient(int clientSocket);
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