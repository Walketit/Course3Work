/**
 * @file DatabaseManager.h
 * @brief Обертка над SQLite для работы с базой данных мессенджера.
 */
#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <string>
#include <sqlite3.h>
#include "common/Logger.h"

/**
 * @brief Класс для управления базой данных (SQLite3).
 * Реализован паттерн Singleton для безопасного доступа к файлу БД.
 */
class DatabaseManager {
private:
    DatabaseManager() = default;
    ~DatabaseManager();
    
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    sqlite3* db = nullptr; // Указатель на объект базы данных SQLite
public:
    static DatabaseManager& getInstance();

    /**
     * @brief Инициализация БД и создание необходимых таблиц.
     * @param dbPath Путь к файлу базы данных.
     * @return true в случае успеха, false при ошибке.
     */
    bool init(const std::string& dbPath = "messenger.db");

    /**
     * @brief Корректное закрытие соединения с БД.
     */
    void close();

};

#endif