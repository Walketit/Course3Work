#include "client/Client.h"
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

Client::Client() : clientSocket(-1), isConnected(false) {}

Client::~Client() {
    disconnect();
}

bool Client::connectToServer(const std::string& ip, uint16_t port) {
    // Создаем сокет
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        std::cerr << "[ОШИБКА] Не удалось создать сокет." << std::endl;
        return false;
    }

    // Настраиваем адрес сервера
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    // inet_pton (Presentation to Network) конвертирует строку IP в байты
    if (inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr) <= 0) {
        std::cerr << "[ОШИБКА] Неверный IP-адрес." << std::endl;
        return false;
    }

    // Устанавливаем соединение (TCP Handshake)
    if (connect(clientSocket, reinterpret_cast<struct sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
        std::cerr << "[ОШИБКА] Подключение к серверу отклонено." << std::endl;
        return false;
    }

    isConnected = true;
    std::cout << "[INFO] Успешно подключено к серверу " << ip << ":" << port << std::endl;
    return true;
}

void Client::disconnect() {
    if (isConnected) {
        close(clientSocket);
        clientSocket = -1;
        isConnected = false;
        std::cout << "[INFO] Отключено от сервера." << std::endl;
    }
}

bool Client::sendData(const std::string& data) {
    if (!isConnected) return false;
    
    // Отправляем строку
    ssize_t bytesSent = send(clientSocket, data.c_str(), data.length() + 1, 0);
    return bytesSent > 0;
}

std::string Client::receiveData() {
    if (!isConnected) return "";

    char buffer[4096]; // Буфер для приема сообщений (4 Килобайта)
    memset(buffer, 0, sizeof(buffer));

    // recv блокирует программу, пока сервер что-нибудь не пришлет
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesRead > 0) {
        return std::string(buffer); // Превращаем массив char обратно в std::string
    } else {
        std::cerr << "[INFO] Сервер разорвал соединение." << std::endl;
        disconnect();
        return "";
    }
}