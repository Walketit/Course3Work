#include "server/Server.h"
#include "server/DatabaseManager.h"
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

    // SO_REUSEADDR предотвращает ошибку "Address already in use" при быстром перезапуске сервера
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

    // Локальное состояние сессии: сервер знает, кто именно сидит на этом сокете
    int currentUserId = -1;
    char buffer[4096];

    // Бесконечный цикл обработки (пока клиент не отключится)
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesRead <= 0) {
            // Если bytesRead == 0, клиент штатно закрыл соединение. Если < 0 - ошибка сети.
            Logger::getInstance().log("Клиент отключился (сокет " + std::to_string(clientSocket) + ")", LogLevel::INFO);
            break; // Выходим из цикла, завершаем поток
        }

        std::string receivedData(buffer);
        Logger::getInstance().log("Сырые данные: " + receivedData, LogLevel::DEBUG);

        try {
            Packet incomingPacket = Packet::deserialize(receivedData);
            Packet response;

            // Маршрутизация пакетов
            
            // Регистрация нового пользователя
            if (incomingPacket.type == PacketType::REGISTER) {
                std::string username = incomingPacket.payload["username"];
                std::string password = incomingPacket.payload["password"];
                
                if (DatabaseManager::getInstance().registerUser(username, password)) {
                    response.type = PacketType::SUCCESS_RESPONSE;
                    response.payload["message"] = "Регистрация успешна!";
                } else {
                    response.type = PacketType::ERROR_RESPONSE;
                    response.payload["message"] = "Ошибка: Логин уже занят.";
                }
            } 
            // Авторизация пользователя
            else if (incomingPacket.type == PacketType::LOGIN) {
                std::string username = incomingPacket.payload["username"];
                std::string password = incomingPacket.payload["password"];
                
                int userId = DatabaseManager::getInstance().authenticateUser(username, password);
                if (userId != -1) {
                    currentUserId = userId; // Авторизация прошла успешно, запоминаем ID

                    // Регистрируем сокет в глобальной таблице маршрутизации
                    {
                        std::lock_guard<std::mutex> lock(clientsMutex);
                        activeClients[userId] = clientSocket;
                    }
                    
                    response.type = PacketType::SUCCESS_RESPONSE;
                    response.payload["user_id"] = userId;
                    response.payload["message"] = "Успешный вход!";
                } else {
                    response.type = PacketType::ERROR_RESPONSE;
                    response.payload["message"] = "Неверный логин или пароль.";
                }
            }
            // Создание нового чата
            else if (incomingPacket.type == PacketType::CREATE_CHAT) {
                int senderId = incomingPacket.payload["sender_id"];
                std::string targetUsername = incomingPacket.payload["target_username"];

                int targetId = DatabaseManager::getInstance().getUserIdByUsername(targetUsername);

                if (targetId == -1) {
                    response.type = PacketType::ERROR_RESPONSE;
                    response.payload["message"] = "Пользователь '" + targetUsername + "' не найден.";
                } else if (senderId == targetId) {
                    response.type = PacketType::ERROR_RESPONSE;
                    response.payload["message"] = "Нельзя создать чат с самим собой.";
                } else {
                    // Сначала проверяем существует ли чат между этими пользователями,
                    // если нет, создаём новый
                    int existingChatId = DatabaseManager::getInstance().getPersonalChat(senderId, targetId);

                    if (existingChatId != -1) {
                        // Чат уже существует, выдаем его ID без создания нового
                        response.type = PacketType::SUCCESS_RESPONSE;
                        response.payload["chat_id"] = existingChatId;
                        response.payload["message"] = "Чат с " + targetUsername + " уже существует. Вы вошли в него.";
                    } else {
                        // Чата нет, создаем новый
                        int newChatId = DatabaseManager::getInstance().createPersonalChat();
                        if (newChatId != -1) {
                            DatabaseManager::getInstance().addChatMember(newChatId, senderId);
                            DatabaseManager::getInstance().addChatMember(newChatId, targetId);

                            response.type = PacketType::SUCCESS_RESPONSE;
                            response.payload["chat_id"] = newChatId;
                            response.payload["message"] = "Чат с " + targetUsername + " успешно создан!";
                        } else {
                            response.type = PacketType::ERROR_RESPONSE;
                            response.payload["message"] = "Ошибка БД при создании чата.";
                        }
                    }
                }
            }
            else if (incomingPacket.type == PacketType::SEND_MESSAGE) {
                int senderId = incomingPacket.payload["sender_id"];
                int chatId = incomingPacket.payload["chat_id"];
                std::string text = incomingPacket.payload["text"];

                // Атомарное сохранение в базу данных
                bool isSaved = DatabaseManager::getInstance().saveMessage(chatId, senderId, text);
                if (isSaved) {
                    response.type = PacketType::SUCCESS_RESPONSE;
                    response.payload["status"] = "OK";
                    response.payload["message"] = "Сообщение сохранено";
                    int targetUserId = DatabaseManager::getInstance().getOtherChatMember(chatId, senderId);
                    
                    if (targetUserId != -1) {
                        int targetSocket = -1;
                        
                        // Ищем, подключен ли получатель прямо сейчас
                        {
                            std::lock_guard<std::mutex> lock(clientsMutex);
                            if (activeClients.find(targetUserId) != activeClients.end()) {
                                targetSocket = activeClients[targetUserId];
                            }
                        }

                        // Если получатель онлайн, отправляем ему пакет NEW_MESSAGE в его сокет
                        if (targetSocket != -1) {
                            Packet pushPacket;
                            pushPacket.type = PacketType::NEW_MESSAGE;
                            pushPacket.payload["chat_id"] = chatId;
                            pushPacket.payload["sender_id"] = senderId;
                            pushPacket.payload["text"] = text;

                            std::string pushStr = pushPacket.serialize();
                            send(targetSocket, pushStr.c_str(), pushStr.length() + 1, 0);
                            Logger::getInstance().log("Сообщение переслано пользователю " + std::to_string(targetUserId), LogLevel::INFO);
                        }
                    }
                } else {
                    response.type = PacketType::ERROR_RESPONSE;
                    response.payload["message"] = "Ошибка БД при сохранении сообщения";
                }
            }
            // Запрос клиента на получение списка его чатов.
            else if (incomingPacket.type == PacketType::GET_CHATS) {
                int userId = incomingPacket.payload["user_id"];
                
                // Достаем вектор чатов из БД
                auto chats = DatabaseManager::getInstance().getUserChats(userId);

                // Формируем JSON-массив
                json chatArray = json::array();
                for (const auto& chat : chats) {
                    chatArray.push_back({
                        {"chat_id", chat.chatId}, 
                        {"chat_name", chat.chatName}
                    });
                }

                response.type = PacketType::CHAT_LIST_RESPONSE;
                response.payload["chats"] = chatArray;
                response.payload["message"] = "Список чатов получен";
            }
            // Запрос клиента на получение списка сообщений в выбранном чате.
            else if (incomingPacket.type == PacketType::GET_CHAT_HISTORY) {
                int chatId = incomingPacket.payload["chat_id"];
                
                // Достаем вектор сообщений из БД
                auto history = DatabaseManager::getInstance().getChatHistory(chatId);

                // Формируем JSON-массив сообщений
                json historyArray = json::array();
                for (const auto& msg : history) {
                    historyArray.push_back({
                        {"sender_id", msg.senderId},
                        {"content", msg.content},
                        {"timestamp", msg.timestamp}
                    });
                }

                response.type = PacketType::HISTORY_RESPONSE;
                response.payload["history"] = historyArray;
                response.payload["message"] = "История сообщений получена";
            }           

            // Отправляем ответ обратно клиенту
            std::string responseStr = response.serialize();
            send(clientSocket, responseStr.c_str(), responseStr.length() + 1, 0);

        } catch (const std::exception& e) {
            Logger::getInstance().log("Ошибка JSON: " + std::string(e.what()), LogLevel::ERROR);
            Packet error;
            error.type = PacketType::ERROR_RESPONSE;
            error.payload["message"] = "Неверный формат пакета";
            std::string errorStr = error.serialize();
            send(clientSocket, errorStr.c_str(), errorStr.length() + 1, 0);
        }
    }

    // При отключении удаляем из таблицы маршрутизации
    if (currentUserId != -1) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        activeClients.erase(currentUserId);
        Logger::getInstance().log("Пользователь " + std::to_string(currentUserId) + " удален из таблицы маршрутизации.", LogLevel::INFO);
    }

    close(clientSocket);
    Logger::getInstance().log("Поток завершен.", LogLevel::DEBUG);
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