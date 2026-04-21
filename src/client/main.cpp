/**
 * @file main.cpp
 * @brief Главный файл клиентской части.
 */
#include "client/Client.h"
#include "common/Packet.h"
#include <iostream>
#include <string>

// Перечисление экранов (состояний) нашего приложения
enum class AppState {
    AUTH,
    MAIN_MENU,
    IN_CHAT
};

int main() {
    Client client;
    if (!client.connectToServer("127.0.0.1", 8080)) {
        return 1;
    }

    AppState currentState = AppState::AUTH;
    int myUserId = -1;
    int myChatId = -1;
    std::string currentChatName = "";

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

            std::string responseData = client.receiveData();
            if (responseData.empty()) break;

            Packet resp = Packet::deserialize(responseData);
            std::cout << ">>> " << resp.payload["message"].get<std::string>() << "\n";

            // Если вход успешен, переходим в главное меню
            if (resp.type == PacketType::SUCCESS_RESPONSE && choice == 2) {
                myUserId = resp.payload["user_id"];
                currentState = AppState::MAIN_MENU;
            }
        } 
        else if (currentState == AppState::MAIN_MENU) {
            std::cout << "\n=== ГЛАВНОЕ МЕНЮ (Ваш ID: " << myUserId << ") ===\n";
            std::cout << "1. Мои чаты\n2. Создать новый чат\n3. Выйти из аккаунта\nВыберите действие: ";
            int choice;
            if (!(std::cin >> choice)) break;

            if (choice == 3) {
                myUserId = -1; // Сбрасываем сессию
                currentState = AppState::AUTH;
                continue;
            }

            if (choice == 1) {
                // Запрашиваем список чатов
                Packet req;
                req.type = PacketType::GET_CHATS;
                req.payload["user_id"] = myUserId;
                client.sendData(req.serialize());

                std::string responseData = client.receiveData();
                if (responseData.empty()) break;
                
                Packet resp = Packet::deserialize(responseData);
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
                        currentState = AppState::IN_CHAT; // Переходим в окно чата
                    }
                }
            }
            else if (choice == 2) {
                std::string targetUsername;
                std::cout << "Введите логин пользователя: "; 
                std::cin >> targetUsername;

                Packet req;
                req.type = PacketType::CREATE_CHAT;
                req.payload["sender_id"] = myUserId;
                req.payload["target_username"] = targetUsername;
                client.sendData(req.serialize());

                std::string responseData = client.receiveData();
                if (responseData.empty()) break;

                Packet resp = Packet::deserialize(responseData);
                std::cout << ">>> " << resp.payload["message"].get<std::string>() << "\n";
                
                if (resp.type == PacketType::SUCCESS_RESPONSE) {
                    myChatId = resp.payload["chat_id"];
                    currentChatName = targetUsername;
                    currentState = AppState::IN_CHAT; // Сразу переходим в чат
                }
            }
        }
        else if (currentState == AppState::IN_CHAT) {
            // 1. При входе в это состояние всегда сначала запрашиваем историю
            Packet reqHistory;
            reqHistory.type = PacketType::GET_CHAT_HISTORY;
            reqHistory.payload["chat_id"] = myChatId;
            client.sendData(reqHistory.serialize());

            std::string responseData = client.receiveData();
            if (responseData.empty()) break;

            Packet resp = Packet::deserialize(responseData);
            
            // 2. Отрисовываем историю
            std::cout << "\n=== ЧАТ: " << currentChatName << " ===\n";
            if (resp.type == PacketType::HISTORY_RESPONSE) {
                auto history = resp.payload["history"];
                if (history.empty()) {
                    std::cout << "Здесь пока нет сообщений...\n";
                } else {
                    for (const auto& msg : history) {
                        int sender = msg["sender_id"];
                        std::string text = msg["content"];
                        std::string time = msg["timestamp"];
                        
                        // Если ID отправителя совпадает с нашим, пишем "Вы", иначе имя собеседника
                        std::string author = (sender == myUserId) ? "Вы" : currentChatName;
                        std::cout << "[" << time << "] " << author << ": " << text << "\n";
                    }
                }
            }
            std::cout << "----------------------\n";
            
            // 3. Ждем ввода сообщения (или специальных команд)
            std::cout << "Введите сообщение ( /back - выйти, /update - обновить ):\n> ";
            std::string text;
            
            // std::ws очищает лишние пробелы и переносы строк, оставшиеся в буфере после std::cin
            std::getline(std::cin >> std::ws, text);

            if (text == "/back") {
                currentState = AppState::MAIN_MENU; // Выходим из чата обратно в список
                continue;
            } else if (text == "/update") {
                continue; // Цикл пойдет заново и просто подтянет новую историю
            }

            // 4. Отправляем сообщение
            Packet reqSend;
            reqSend.type = PacketType::SEND_MESSAGE;
            reqSend.payload["sender_id"] = myUserId;
            reqSend.payload["chat_id"] = myChatId;
            reqSend.payload["text"] = text;
            client.sendData(reqSend.serialize());

            // Ждем техническое подтверждение от сервера, чтобы не рассинхронизироваться
            client.receiveData(); 
        }
    }

    client.disconnect();
    return 0;
}