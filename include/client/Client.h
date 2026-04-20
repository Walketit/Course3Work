/**
 * @file Client.h
 * @brief Класс сетевого клиента для подключения к серверу.
 */
#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <cstdint>

class Client {
private:
    int clientSocket;
    bool isConnected;
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
     */
    bool sendData(const std::string& data);

    /**
     * @brief Получение данных от сервера.
     */
    std::string receiveData();
};

#endif