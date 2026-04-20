#include "server/Server.h"
#include "common/Logger.h"

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
    Logger::getInstance().log("Поток запущен для сокета " + std::to_string(clientSocket), LogLevel::DEBUG);

    std::string welcomeMsg = "Привет! Ты в отдельном потоке. Ждем 10 секунд...\n";
    send(clientSocket, welcomeMsg.c_str(), welcomeMsg.length(), 0);

    // Искусственная задержка, чтобы имитировать работу
    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::string byeMsg = "Время вышло. Пока!\n";
    send(clientSocket, byeMsg.c_str(), byeMsg.length(), 0);

    close(clientSocket);
    Logger::getInstance().log("Клиент отключен, поток завершен.", LogLevel::DEBUG);
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