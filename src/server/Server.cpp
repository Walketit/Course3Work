#include "server/Server.h"
#include "common/Logger.h"
#include "common/Packet.h"
// Системные библиотеки POSIX для работы с сетью
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

Server::Server(uint16_t port) : port(port), serverSocket(-1), isRunning(false) {}

Server::~Server() {
    stop();
}

void Server::start() {
    // Создание сокета
    // AF_INET - используем IPv4
    // SOCK_STREAM - используем протокол TCP
    // 0 - IP-протокол по умолчанию
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        Logger::getInstance().log("Ошибка создания сокета!", LogLevel::ERROR);
        return;
    }

    // Настройка сокета (SO_REUSEADDR)
    // Если сервер упадет, ОС может заморозить порт на пару минут.
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        Logger::getInstance().log("Ошибка настройки setsockopt", LogLevel::ERROR);
        return;
    }

    // Подготовка структуры с адресом
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY; // Слушаем на всех сетевых интерфейсах
    
    // htons (Host TO Network Short) - переводит число порта в сетевой порядок байт (Big-Endian)
    serverAddress.sin_port = htons(port);

    // Привязка сокета к порту (Bind)
    if (bind(serverSocket, reinterpret_cast<struct sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
        Logger::getInstance().log("Ошибка привязки (bind) к порту " + std::to_string(port), LogLevel::ERROR);
        return;
    }

    // Перевод сокета в режим прослушивания (Listen)
    // SOMAXCONN - максимальная длина очереди ожидающих клиентов (задает сама ОС)
    if (listen(serverSocket, SOMAXCONN) < 0) {
        Logger::getInstance().log("Ошибка прослушивания (listen)", LogLevel::ERROR);
        return;
    }

    isRunning = true;
    Logger::getInstance().log("Сервер успешно запущен. Ожидание подключений на порту " + std::to_string(port) + "...", LogLevel::INFO);

    // Главный цикл сервера (Accept)
    while (isRunning) {
        sockaddr_in clientAddress{};
        socklen_t clientLen = sizeof(clientAddress);

        // accept блокирует программу, пока кто-нибудь не подключится по сети
        int clientSocket = accept(serverSocket, reinterpret_cast<struct sockaddr*>(&clientAddress), &clientLen);
        
        if (clientSocket < 0) {
            if (isRunning) {
                Logger::getInstance().log("Ошибка при принятии подключения (accept)", LogLevel::ERROR);
            }
            continue;
        }

        Logger::getInstance().log("Подключился новый клиент!", LogLevel::INFO);
        
        // Реализация многопоточности
        // Создаем новый поток, передаем ему указатель на метод handleClient, 
        // объект сервера (this) и номер сокета клиента.
        std::thread clientThread(&Server::handleClient, this, clientSocket);
        
        // detach() отрывает поток от основного управления. 
        // Поток будет работать отдельно и сам очистит память, когда завершит работу.
        clientThread.detach(); 
    }
}

void Server::handleClient(int clientSocket) {
    Logger::getInstance().log("Поток запущен для обслуживания сокета " + std::to_string(clientSocket), LogLevel::DEBUG);

    // Сначала отправим приветствие
    std::string welcome = "Добро пожаловать на сервер! Ожидаю ваш JSON пакет...\n";
    send(clientSocket, welcome.c_str(), welcome.length() + 1, 0);

    // Буфер для приема данных
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));

    // Читаем данные из сокета (recv блокирует поток, пока данные не придут)
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytesRead > 0) {
        std::string receivedData(buffer);
        Logger::getInstance().log("Сырые данные от клиента: " + receivedData, LogLevel::DEBUG);

        try {
            // Десериализация: превращаем строку в объект Packet
            Packet incomingPacket = Packet::deserialize(receivedData);

            // Обработка в зависимости от типа пакета
            if (incomingPacket.type == PacketType::SEND_MESSAGE) {
                int senderId = incomingPacket.payload["sender_id"];
                std::string text = incomingPacket.payload["text"];

                Logger::getInstance().log("ПОЛУЧЕНО СООБЩЕНИЕ: [От ID " + std::to_string(senderId) + "]: " + text, LogLevel::INFO);

                // Отправляем ответ клиенту, что всё прошло успешно
                Packet response;
                response.type = PacketType::SUCCESS_RESPONSE;
                response.payload["status"] = "OK";
                response.payload["info"] = "Сообщение обработано сервером";

                std::string responseStr = response.serialize();
                send(clientSocket, responseStr.c_str(), responseStr.length() + 1, 0);
            }
        } catch (const std::exception& e) {
            Logger::getInstance().log("Ошибка обработки JSON: " + std::string(e.what()), LogLevel::ERROR);
            
            Packet error;
            error.type = PacketType::ERROR_RESPONSE;
            error.payload["message"] = "Неверный формат JSON";
            std::string errorStr = error.serialize();
            send(clientSocket, errorStr.c_str(), errorStr.length() + 1, 0);
        }
    } else if (bytesRead == 0) {
        Logger::getInstance().log("Клиент разорвал соединение до отправки данных.", LogLevel::WARNING);
    } else {
        Logger::getInstance().log("Ошибка при чтении из сокета (recv).", LogLevel::ERROR);
    }

    // Завершение работы с клиентом
    close(clientSocket);
    Logger::getInstance().log("Поток завершен, сокет закрыт.", LogLevel::DEBUG);
}

void Server::stop() {
    if (isRunning) {
        isRunning = false;
        if (serverSocket != -1) {
            close(serverSocket); // Закрываем главный сокет
            serverSocket = -1;
        }
        Logger::getInstance().log("Сервер остановлен.", LogLevel::INFO);
    }
}