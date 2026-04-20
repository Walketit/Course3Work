#include "common/Logger.h"
#include "server/DatabaseManager.h"
#include "server/Server.h"
#include <iostream>

int main() {
    Logger::getInstance().log("Инициализация сервера...");

    // Подключаем БД
    auto& db = DatabaseManager::getInstance();
    if (!db.init("messenger.db")) {
        return 1;
    }

    // Создаем и запускаем сетевой сервер на порту 8080
    Server server(8080);
    server.start(); // Программа зависнет внутри этого метода, бесконечно принимая клиентов

    // Сюда код дойдет только если сервер будет остановлен (server.stop())
    db.close();
    return 0;
}