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
    serverAddress.sin_port = htons(port); // Перевод порта в сетевой порядок байт

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
        // Закрываем сокет. Это мгновенно прервет функцию recv() в фоновом потоке.
        shutdown(clientSocket, SHUT_RDWR);
        close(clientSocket);
        clientSocket = -1;
        isConnected = false;
        
        // Будим главный поток, если он вдруг завис в waitForResponse
        cv.notify_all(); 
    }

    // Ждем, пока фоновый поток безопасно завершит свой цикл while и выйдет
    if (listenerThread.joinable()) {
        listenerThread.join();
    }
}

bool Client::sendData(const std::string& data) {
    if (!isConnected) return false;
    
    // Отправляем строку
    ssize_t bytesSent = send(clientSocket, data.c_str(), data.length() + 1, 0);
    return bytesSent > 0;
}

void Client::startListening(std::function<void(const Packet&)> onNewMessage) {
    if (!isConnected) return;
    
    // Запускаем поток, передавая указатель на метод listenLoop и callback-функцию
    listenerThread = std::thread(&Client::listenLoop, this, onNewMessage);
}

void Client::listenLoop(std::function<void(const Packet&)> onNewMessage) {
    char buffer[4096]; // 4КБ буфер для входящих JSON-строк
    
    while (isConnected) {
        memset(buffer, 0, sizeof(buffer));
        // Блокирующий вызов, поток засыпает здесь до прихода данных
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesRead > 0) {
            try {
                std::string receivedData(buffer);
                Packet incomingPacket = Packet::deserialize(receivedData);

                // Диспетчеризация пакетов (Маршрутизатор на стороне клиента)
                if (incomingPacket.type == PacketType::NEW_MESSAGE) {
                    // Асинхронное событие, немедленно вызываем callback для обновления UI
                    if (onNewMessage) {
                        onNewMessage(incomingPacket);
                    }
                } else {
                    // Иначе это ответ на запрос главного потока. Кладем в очередь.
                    std::lock_guard<std::mutex> lock(queueMutex);
                    responseQueue.push(incomingPacket);
                    cv.notify_one(); // Будим главный поток, висящий в waitForResponse()
                }
            } catch (const std::exception& e) {
                std::cerr << "[ОШИБКА КЛИЕНТА] Не удалось распарсить пакет: " << e.what() << std::endl;
            }
        } else {
            // Ошибка сети или сервер отключился
            std::cerr << "\n[СИСТЕМА] Потеряно соединение с сервером.\n";
            isConnected = false;
            cv.notify_all(); // Разблокируем главный поток при обрыве сети
            break;
        }
    }
}

Packet Client::waitForResponse() {
    std::unique_lock<std::mutex> lock(queueMutex);
    
    // Поток спит, пока очередь пуста И клиент подключен
    // Лямбда-выражение предотвращает ложные пробуждения
    cv.wait(lock, [this]() { return !responseQueue.empty() || !isConnected; });

    if (!isConnected && responseQueue.empty()) {
        Packet errorPacket;
        errorPacket.type = PacketType::ERROR_RESPONSE;
        return errorPacket;
    }

    // Забираем пакет из очереди
    Packet resp = responseQueue.front();
    responseQueue.pop();
    
    return resp;
}