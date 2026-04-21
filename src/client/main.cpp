/**
 * @file main.cpp
 * @brief Главный файл клиентской части. Реализует машину состояний и консольный UI.
 */
#include "client/Client.h"
#include "common/Packet.h"
#include <iostream>
#include <string>
#include <atomic>

// Состояния интерфейса приложения
enum class AppState {
    AUTH,       // Экран логина/регистрации
    MAIN_MENU,  // Экран списка чатов
    IN_CHAT     // Экран внутри конкретного диалога
};

int main() {
    Client client;
    if (!client.connectToServer("127.0.0.1", 8080)) {
        return 1;
    }

    // std::atomic гарантирует, что чтение этих переменных из фонового потока (в onNewMessage)
    // и запись в них из главного потока не приведут к состоянию гонки.
    std::atomic<AppState> currentState{AppState::AUTH};
    std::atomic<int> myUserId{-1};
    std::atomic<int> myChatId{-1};
    std::string currentChatName = "";

    // Callback-функция, которая будет асинхронно вызвана из потока listenLoop
    auto onNewMessage = [&](const Packet& pkt) {
        int chatId = pkt.payload["chat_id"];
        int senderId = pkt.payload["sender_id"];
        std::string text = pkt.payload["text"];

        std::cout << "\n"; // Спускаемся на новую строку, чтобы не сломать ввод пользователя
        
        // Маршрутизация уведомления в зависимости от текущего экрана
        if (currentState == AppState::IN_CHAT && chatId == myChatId) {
            std::cout << "[Новое сообщение] " << currentChatName << ": " << text << "\n";
        } else {
            // Если мы в меню или в другом чате
            std::cout << "[УВЕДОМЛЕНИЕ] Вам прислали сообщение в чат ID " << chatId << "!\n";
        }
        std::cout << "> " << std::flush; // Возвращаем стрелочку ввода
    };

    // Запуск асинхронного слушателя
    client.startListening(onNewMessage);

    // Главный цикл машины состояний (выполняется в главном потоке) 
    while (true) {
        if (currentState == AppState::AUTH) {
            std::cout << "\n=== АВТОРИЗАЦИЯ ===\n";
            std::cout << "1. Регистрация\n2. Вход\n3. Выход\nВыберите действие: ";
            int choice;
            if (!(std::cin >> choice)) break;

            if (choice == 3) break;

            Packet req;
            std::string login, pass;
            std::cout << "Логин: "; std::cin >> login;
            std::cout << "Пароль: "; std::cin >> pass;

            if (choice == 1) req.type = PacketType::REGISTER;
            else if (choice == 2) req.type = PacketType::LOGIN;
            else continue;

            req.payload["username"] = login;
            req.payload["password"] = pass;
            client.sendData(req.serialize());

            // Ждем ответа из очереди пакетов
            Packet resp = client.waitForResponse();
            if (resp.type == PacketType::ERROR_RESPONSE && resp.payload.is_null()) break; // Сервер упал

            std::cout << ">>> " << resp.payload["message"].get<std::string>() << "\n";

            if (resp.type == PacketType::SUCCESS_RESPONSE && choice == 2) {
                myUserId = resp.payload["user_id"];
                currentState = AppState::MAIN_MENU; // Переход на основное меню при успешной аутентификации
            }
        } 
        else if (currentState == AppState::MAIN_MENU) {
            std::cout << "\n=== ГЛАВНОЕ МЕНЮ (Ваш ID: " << myUserId << ") ===\n";
            std::cout << "1. Мои чаты\n2. Создать новый чат\n3. Выйти из аккаунта\nВыберите действие: ";
            int choice;
            if (!(std::cin >> choice)) break;

            if (choice == 3) {
                myUserId = -1;
                currentState = AppState::AUTH;
                continue;
            }

            if (choice == 1) {
                Packet req;
                req.type = PacketType::GET_CHATS;
                req.payload["user_id"] = (int)myUserId;
                client.sendData(req.serialize());

                Packet resp = client.waitForResponse();
                if (resp.type == PacketType::ERROR_RESPONSE && resp.payload.is_null()) break;
                
                if (resp.type == PacketType::CHAT_LIST_RESPONSE) {
                    auto chats = resp.payload["chats"];
                    if (chats.empty()) {
                        std::cout << "У вас пока нет чатов.\n";
                        continue;
                    }

                    std::cout << "\n--- ВАШИ ЧАТЫ ---\n";
                    for (size_t i = 0; i < chats.size(); ++i) {
                        std::cout << "[" << i + 1 << "] " << chats[i]["chat_name"].get<std::string>() << "\n";
                    }
                    std::cout << "[0] Назад\nВыберите чат: ";
                    
                    int chatIdx;
                    std::cin >> chatIdx;
                    
                    if (chatIdx > 0 && chatIdx <= chats.size()) {
                        myChatId = chats[chatIdx - 1]["chat_id"];
                        currentChatName = chats[chatIdx - 1]["chat_name"];
                        currentState = AppState::IN_CHAT; // Переход в чат
                    }
                }
            }
            else if (choice == 2) {
                std::string targetUsername;
                std::cout << "Введите логин пользователя: "; 
                std::cin >> targetUsername;

                Packet req;
                req.type = PacketType::CREATE_CHAT;
                req.payload["sender_id"] = (int)myUserId;
                req.payload["target_username"] = targetUsername;
                client.sendData(req.serialize());

                Packet resp = client.waitForResponse();
                if (resp.type == PacketType::ERROR_RESPONSE && resp.payload.is_null()) break;

                std::cout << ">>> " << resp.payload["message"].get<std::string>() << "\n";
                
                if (resp.type == PacketType::SUCCESS_RESPONSE) {
                    myChatId = resp.payload["chat_id"];
                    currentChatName = targetUsername;
                    currentState = AppState::IN_CHAT;
                }
            }
        }
        else if (currentState == AppState::IN_CHAT) {
            // Запрос истории при первом входе на экран чата
            Packet reqHistory;
            reqHistory.type = PacketType::GET_CHAT_HISTORY;
            reqHistory.payload["chat_id"] = (int)myChatId;
            client.sendData(reqHistory.serialize());

            Packet respHistory = client.waitForResponse();
            if (respHistory.type == PacketType::ERROR_RESPONSE && respHistory.payload.is_null()) break;

            std::cout << "\n=== ЧАТ: " << currentChatName << " ===\n";
            if (respHistory.type == PacketType::HISTORY_RESPONSE) {
                auto history = respHistory.payload["history"];
                if (history.empty()) {
                    std::cout << "Здесь пока нет сообщений...\n";
                } else {
                    for (const auto& msg : history) {
                        int sender = msg["sender_id"];
                        std::string text = msg["content"];
                        std::string time = msg["timestamp"];
                        
                        std::string author = (sender == myUserId) ? "Вы" : currentChatName;
                        std::cout << "[" << time << "] " << author << ": " << text << "\n";
                    }
                }
            }
            std::cout << "----------------------\n";
            
            // Вложенный цикл чата (остаемся на этом экране, пока не введут /back)
            while (currentState == AppState::IN_CHAT) {
                std::cout << "Введите сообщение ( /back - выйти ):\n> ";
                std::string text;
                std::getline(std::cin >> std::ws, text);

                if (text == "/back") {
                    currentState = AppState::MAIN_MENU;
                    break; 
                }

                Packet reqSend;
                reqSend.type = PacketType::SEND_MESSAGE;
                reqSend.payload["sender_id"] = (int)myUserId;
                reqSend.payload["chat_id"] = (int)myChatId;
                reqSend.payload["text"] = text;
                client.sendData(reqSend.serialize());

                // Ждем техническое подтверждение (чтобы убедиться, что записалось в БД)
                client.waitForResponse(); 
            }
        }
    }

    client.disconnect();
    return 0;
}