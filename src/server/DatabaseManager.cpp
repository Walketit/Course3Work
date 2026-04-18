#include "server/DatabaseManager.h"

DatabaseManager& DatabaseManager::getInstance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::~DatabaseManager() {
    close();
}

bool DatabaseManager::init(const std::string& dbPath) {
    // Открываем базу данных. Если файла нет, SQLite создаст его.
    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc) {
        Logger::getInstance().log("Ошибка открытия БД: " + std::string(sqlite3_errmsg(db)), LogLevel::ERROR);
        return false;
    }
    Logger::getInstance().log("База данных успешно открыта: " + dbPath, LogLevel::INFO);

    // Включаем поддержку внешних ключей (Foreign Keys)
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    // SQL-запрос для создания структуры базы данных
    const char* sqlSchema = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS chats (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            type TEXT NOT NULL, -- 'personal' или 'group'
            name TEXT
        );

        CREATE TABLE IF NOT EXISTS chat_members (
            chat_id INTEGER NOT NULL,
            user_id INTEGER NOT NULL,
            FOREIGN KEY (chat_id) REFERENCES chats(id) ON DELETE CASCADE,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
            PRIMARY KEY (chat_id, user_id)
        );

        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            chat_id INTEGER NOT NULL,
            sender_id INTEGER NOT NULL,
            content TEXT NOT NULL,
            reply_to_id INTEGER,
            forward_from_id INTEGER,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (chat_id) REFERENCES chats(id) ON DELETE CASCADE,
            FOREIGN KEY (sender_id) REFERENCES users(id) ON DELETE CASCADE
        );
    )";

    char* errMsg = nullptr;
    rc = sqlite3_exec(db, sqlSchema, nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        Logger::getInstance().log("Ошибка инициализации таблиц: " + std::string(errMsg), LogLevel::ERROR);
        sqlite3_free(errMsg);
        return false;
    }

    Logger::getInstance().log("Таблицы базы данных проверены/созданы.", LogLevel::INFO);
    return true;
}

void DatabaseManager::close() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
        Logger::getInstance().log("Соединение с БД закрыто.", LogLevel::INFO);
    }
}