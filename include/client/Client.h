/**
 * @file Client.h
 * @brief Класс сетевого клиента для подключения к серверу.
 */
#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <cstdint>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include "common/Packet.h"

class Client {
private:
    int clientSocket;
    bool isConnected;

    // Инструменты для многопоточности
    std::thread listenerThread;           // Фоновый поток для прослушивания сети
    std::queue<Packet> responseQueue;     // Очередь для хранения системных ответов (SUCCESS, ERROR и т.д.)
    std::mutex queueMutex;                // Защищает responseQueue от одновременного доступа
    std::condition_variable cv;           // Сигнализирует главному потоку о появлении новых данных в очереди
    
    // Внутренняя функция фонового потока
    void listenLoop(std::function<void(const Packet&)> onNewMessage);
public:
    Client();
    ~Client();

    /**
     * @brief Подключение к серверу.
     * @param ip IP-адрес сервера
     * @param port Порт сервера
     * @return true, если подключение успешно
     */
    bool connectToServer(const std::string& ip, uint16_t port);

    /**
     * @brief Отключение от сервера.
     */
    void disconnect();

    /**
     * @brief Отправка сериализованных данных на сервер.
     * @param data Строка данных.
     */
    bool sendData(const std::string& data);

    // Запуск фонового потока, который слушает сеть. 
    void startListening(std::function<void(const Packet&)> onNewMessage);
    
    // Метод для главного потока: уснуть и ждать ответа от сервера
    Packet waitForResponse();
};

#endif