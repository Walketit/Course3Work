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

bool DatabaseManager::registerUser(const std::string& username, const std::string& passwordHash) {
    // Как только создается lock_guard, он захватывает мьютекс.
    // Если другой поток уже держит мьютекс, этот поток заснет на этой строчке
    // и будет ждать своей очереди.
    std::lock_guard<std::mutex> lock(dbMutex);

    if (!db) return false;

    // Знак '?' означает место, куда мы безопасно вставим переменную
    const char* sql = "INSERT INTO users (username, password_hash) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;

    // Подготовка запроса (компиляция SQL)
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::getInstance().log("Ошибка подготовки запроса registerUser", LogLevel::ERROR);
        return false;
    }

    // Привязка (Binding) переменных вместо знаков '?'
    // SQLITE_TRANSIENT указывает SQLite сделать копию строки, чтобы избежать проблем с памятью
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, passwordHash.c_str(), -1, SQLITE_TRANSIENT);

    // Выполнение запроса
    bool success = false;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        success = true;
        Logger::getInstance().log("Зарегистрирован новый пользователь: " + username, LogLevel::INFO);
    } else {
        // Скорее всего сработал UNIQUE constraint (такой логин уже есть)
        Logger::getInstance().log("Не удалось зарегистрировать пользователя (возможно логин занят): " + username, LogLevel::WARNING);
    }

    // Очистка памяти запроса
    sqlite3_finalize(stmt);
    return success;
}

int DatabaseManager::authenticateUser(const std::string& username, const std::string& passwordHash) {
    std::lock_guard<std::mutex> lock(dbMutex);

    if (!db) return -1;

    const char* sql = "SELECT id FROM users WHERE username = ? AND password_hash = ?;";
    sqlite3_stmt* stmt = nullptr;
    int userId = -1;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::getInstance().log("Ошибка подготовки запроса authenticateUser", LogLevel::ERROR);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, passwordHash.c_str(), -1, SQLITE_TRANSIENT);

    // Если sqlite3_step возвращает SQLITE_ROW, значит найдена строка (пароль совпал)
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        userId = sqlite3_column_int(stmt, 0); // Берем значение из 0-й колонки (id)
    }

    sqlite3_finalize(stmt);
    return userId;
}

int DatabaseManager::createPersonalChat() {
    std::lock_guard<std::mutex> lock(dbMutex);

    if (!db) return -1;

    const char* sql = "INSERT INTO chats (type, name) VALUES ('personal', NULL);";
    
    // Выполняем простой запрос без параметров
    if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
        Logger::getInstance().log("Ошибка создания чата", LogLevel::ERROR);
        return -1;
    }

    // Эта встроенная функция SQLite возвращает ID последней созданной записи
    int chatId = sqlite3_last_insert_rowid(db);
    Logger::getInstance().log("Создан новый чат с ID: " + std::to_string(chatId), LogLevel::INFO);
    
    return chatId;
}

int DatabaseManager::getPersonalChat(int user1Id, int user2Id) {
    std::lock_guard<std::mutex> lock(dbMutex);
    if (!db) return -1;

    // Ищем чат типа 'personal', к которому привязаны записи
    // из таблицы chat_members как для user1, так и для user2 одновременно.
    const char* sql = R"(
        SELECT c.id 
        FROM chats c
        JOIN chat_members cm1 ON c.id = cm1.chat_id
        JOIN chat_members cm2 ON c.id = cm2.chat_id
        WHERE c.type = 'personal' 
          AND cm1.user_id = ? 
          AND cm2.user_id = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    int chatId = -1;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user1Id);
        sqlite3_bind_int(stmt, 2, user2Id);

        // Если нашли совпадение, забираем ID этого чата
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            chatId = sqlite3_column_int(stmt, 0); 
        }
    }
    
    sqlite3_finalize(stmt);
    return chatId; // Вернет -1, если такого чата нет
}

bool DatabaseManager::saveMessage(int chatId, int senderId, const std::string& content) {
    std::lock_guard<std::mutex> lock(dbMutex);
    
    if (!db) return false;

    const char* sql = "INSERT INTO messages (chat_id, sender_id, content) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::getInstance().log("Ошибка подготовки запроса saveMessage", LogLevel::ERROR);
        return false;
    }

    sqlite3_bind_int(stmt, 1, chatId);
    sqlite3_bind_int(stmt, 2, senderId);
    sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_TRANSIENT);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    
    if (success) {
        Logger::getInstance().log("Сообщение сохранено в чат " + std::to_string(chatId), LogLevel::DEBUG);
    } else {
        Logger::getInstance().log("Ошибка сохранения сообщения", LogLevel::ERROR);
    }

    sqlite3_finalize(stmt);
    return success;
}

int DatabaseManager::getUserIdByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(dbMutex);
    if (!db) return -1;

    const char* sql = "SELECT id FROM users WHERE username = ?;";
    sqlite3_stmt* stmt = nullptr;
    int userId = -1;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            userId = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    return userId;
}

bool DatabaseManager::addChatMember(int chatId, int userId) {
    std::lock_guard<std::mutex> lock(dbMutex);
    if (!db) return false;

    const char* sql = "INSERT INTO chat_members (chat_id, user_id) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, chatId);
    sqlite3_bind_int(stmt, 2, userId);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

void DatabaseManager::close() {
    std::lock_guard<std::mutex> lock(dbMutex);

    if (db) {
        sqlite3_close(db);
        db = nullptr;
        Logger::getInstance().log("Соединение с БД закрыто.", LogLevel::INFO);
    }
}