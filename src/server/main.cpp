#include "common/Logger.h"
#include "server/DatabaseManager.h"

int main() {
    Logger::getInstance().log("Сервер запускается...");

    // Инициализация базы данных
    if (!DatabaseManager::getInstance().init("messenger.db")) {
        Logger::getInstance().log("Критическая ошибка: невозможно инициализировать БД. Остановка сервера.", LogLevel::ERROR);
        return 1;
    }

    // Имитация работы сервера
    Logger::getInstance().log("Ожидание подключений (пока не реализовано)...", LogLevel::DEBUG);

    // Перед выходом корректно закрываем БД
    DatabaseManager::getInstance().close();
    
    return 0;
}