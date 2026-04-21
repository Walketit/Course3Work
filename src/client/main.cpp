/**
 * @file main.cpp
 * @brief Главный файл клиентской части.
 */
#include "client/Client.h"
#include "common/Packet.h"
#include <iostream>
#include <string>

int main() {
    Client client;
    if (!client.connectToServer("127.0.0.1", 8080)) {
        return 1;
    }

    // Состояние сессии
    int myUserId = -1;
    int myChatId = -1;

    while (true) {
        std::cout << "\n=== МЕССЕНДЖЕР ===" << std::endl;
        std::cout << "Текущий статус: " << (myUserId != -1 ? "В сети (ID: " + std::to_string(myUserId) + ")" : "Гость") << std::endl;
        std::cout << "Активный чат: " << (myChatId != -1 ? "Чат ID " + std::to_string(myChatId) : "Не выбран") << std::endl;
        std::cout << "1. Регистрация\n2. Вход\n3. Создать новый чат\n4. Отправить сообщение\n5. Выход" << std::endl;
        std::cout << "Выберите действие: ";
        
        int choice;
        std::cin >> choice;

        Packet req;
        if (choice == 1) {
            std::string login, pass;
            std::cout << "Придумайте логин: "; std::cin >> login;
            std::cout << "Придумайте пароль: "; std::cin >> pass;
            
            req.type = PacketType::REGISTER;
            req.payload["username"] = login;
            req.payload["password"] = pass;
        } 
        else if (choice == 2) {
            std::string login, pass;
            std::cout << "Логин: "; std::cin >> login;
            std::cout << "Пароль: "; std::cin >> pass;
            
            req.type = PacketType::LOGIN;
            req.payload["username"] = login;
            req.payload["password"] = pass;
        }
        else if (choice == 3) {
            if (myUserId == -1) {
                std::cout << "[ОШИБКА] Сначала нужно войти в аккаунт!" << std::endl;
                continue;
            }
            std::string targetUsername;
            std::cout << "Введите логин пользователя, с кем хотите начать чат: "; 
            std::cin >> targetUsername;

            req.type = PacketType::CREATE_CHAT;
            req.payload["sender_id"] = myUserId; // Сервер должен знать, кто создает чат
            req.payload["target_username"] = targetUsername; // И с кем
        }
        else if (choice == 4) {
            if (myUserId == -1 || myChatId == -1) {
                std::cout << "[ОШИБКА] Сначала нужно войти в аккаунт и создать чат!" << std::endl;
                continue;
            }
            std::string text;
            std::cout << "Введите сообщение: ";
            std::cin.ignore(); // Очищаем буфер после std::cin
            std::getline(std::cin, text);

            req.type = PacketType::SEND_MESSAGE;
            req.payload["sender_id"] = myUserId;
            req.payload["chat_id"] = myChatId;
            req.payload["text"] = text;
        }
        else if (choice == 5) {
            break;
        } else {
            std::cout << "Неверный ввод." << std::endl;
            continue;
        }

        // Отправляем пакет на сервер
        client.sendData(req.serialize());

        // Ждем ответа
        std::string responseData = client.receiveData();
        if (responseData.empty()) break; // Сервер упал

        Packet resp = Packet::deserialize(responseData);
        std::cout << "\n>>> ОТВЕТ СЕРВЕРА: " << resp.payload["message"].get<std::string>() << std::endl;

        // Сохраняем ID, если сервер их прислал (чтобы использовать их в следующих запросах)
        if (resp.type == PacketType::SUCCESS_RESPONSE) {
            if (choice == 2) myUserId = resp.payload["user_id"];
            if (choice == 3) myChatId = resp.payload["chat_id"];
        }
    }

    client.disconnect();
    return 0;
}