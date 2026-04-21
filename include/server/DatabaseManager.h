/**
 * @file DatabaseManager.h
 * @brief Обертка над SQLite для работы с базой данных мессенджера.
 */
#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <string>
#include <sqlite3.h>
#include <mutex>
#include <vector>
#include "common/Logger.h"

// Структура для информации о чате в списке
struct ChatInfo {
    int chatId;
    std::string chatName; // Логин собеседника
};

// Структура для одного сообщения из истории
struct MessageInfo {
    int senderId;
    std::string content;
    std::string timestamp;
};

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
    std::mutex dbMutex;
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

    /**
     * @brief Регистрация нового пользователя.
     * @param username Логин.
     * @param password Пароль.
     * @return true в случае успеха, false если пользователь уже существует или произошла ошибка.
     */
    bool registerUser(const std::string& username, const std::string& password);

    /**
     * @brief Авторизация пользователя.
     * @param username Логин.
     * @param password Пароль.
     * @return ID пользователя (>0) при успехе, -1 при неверном логине/пароле.
     */
    int authenticateUser(const std::string& username, const std::string& password);

    /**
     * @brief Создает новый личный чат.
     * @return ID созданного чата или -1 при ошибке.
     */
    int createPersonalChat();

    /**
     * @brief Найти существующий личный чат между двумя пользователями.
     * @param user1Id ID первого пользователя
     * @param user2Id ID второго пользователя 
     * @return ID чата пользователей при успехе, -1 если нет.
     */
    int getPersonalChat(int user1Id, int user2Id);

    /**
     * @brief Сохранение сообщения в базу данных.
     * @param chatId ID чата.
     * @param senderId ID отправителя.
     * @param content Текст сообщения.
     * @return true при успешном сохранении.
     */
    bool saveMessage(int chatId, int senderId, const std::string& content);

    /**
     * @brief Получение ID пользователя по логину
     * @param username Логин
     * @return возвращает ID пользователя, либо (-1).
     */
    int getUserIdByUsername(const std::string& username);
    
    /**
     * @brief Добавление пользователя в состав чата.
     * @param chatId ID чата.
     * @param userId ID пользователя.
     * @return true при успешном сохранении.
     */
    bool addChatMember(int chatId, int userId);

    /**
     * @brief Получить список всех чатов пользователя (с именами собеседников).
     * @param userId ID пользователя.
     * @return chats, все существующие в БД чаты пользователя.
     */
    std::vector<ChatInfo> getUserChats(int userId);

    /**
     * @brief Получить историю сообщений конкретного чата.
     * @param chatId ID чата.
     * @return history, все сообщения чата.
     */
    std::vector<MessageInfo> getChatHistory(int chatId);

    /**
     * @brief Получить ID второго пользователя личного чата
     * @param chatId ID чата.
     * @param myUserId ID первого пользователя.
     * @return ID второго пользователя.
     */
    int getOtherChatMember(int chatId, int myUserId);
};

#endif