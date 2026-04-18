#include "common/Logger.h"
#include "server/DatabaseManager.h"
#include <iostream>

int main() {
    Logger::getInstance().log("Сервер запускается...");

    auto& db = DatabaseManager::getInstance();
    if (!db.init("messenger.db")) {
        return 1;
    }

    db.registerUser("admin", "secure_hash_1");

    int myId = db.authenticateUser("admin", "secure_hash_1");
    if (myId != -1) {
        std::cout << "Успешный вход! Мой ID: " << myId << std::endl;
        
        // Создаем чат прямо из кода
        int newChatId = db.createPersonalChat();
        
        if (newChatId != -1) {
            // И теперь гарантированно успешно сохраняем в него сообщение!
            db.saveMessage(newChatId, myId, "Привет из C++ кода! БД работает идеально."); 
        }
    }

    db.close();
    return 0;
}