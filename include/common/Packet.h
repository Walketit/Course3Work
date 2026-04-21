/**
 * @file Packet.h
 * @brief Структура сетевого пакета для обмена между клиентом и сервером.
 * Этот файл определяет протокол, на котором общаются клиент и сервер.
 * Использование JSON позволяет гибко добавлять новые поля без изменения бинарной структуры пакета.
 */
#ifndef PACKET_H
#define PACKET_H

#include <string>
#include "json/json.hpp"

// Для удобства создаем псевдоним
using json = nlohmann::json;

/**
 * @brief Перечисление типов сообщений.
 */
enum class PacketType {
    REGISTER,           // Запрос: Регистрация нового аккаунта
    LOGIN,              // Запрос: Авторизация
    CREATE_CHAT,        // Запрос: Создание нового диалога
    SEND_MESSAGE,       // Запрос: Отправка сообщения на сервер
    NEW_MESSAGE,        // Push-уведомление от сервера: входящее сообщение
    GET_CHATS,          // Запрос: Получение списка диалогов
    GET_CHAT_HISTORY,   // Запрос: Получение истории конкретного чата
    CHAT_LIST_RESPONSE, // Ответ сервера со списком чатов
    HISTORY_RESPONSE,   // Ответ сервера с массивом сообщений
    ERROR_RESPONSE,     // Универсальный ответ об ошибке
    SUCCESS_RESPONSE    // Универсальный ответ об успехе
};

/**
 * @brief Класс, представляющий единицу данных, передаваемую по сети.
 */
struct Packet {
    PacketType type;
    json payload; // Динамическая полезная нагрузка. Может содержать любые ключи.

    // Превращаем структуру C++ в строку JSON (Сериализация)
    std::string serialize() const {
        json j;
        j["type"] = static_cast<int>(type);
        j["payload"] = payload;
        return j.dump(); // dump() превращает JSON-объект в строку
    }

    // Восстанавливаем структуру C++ из строки JSON (Десериализация)
    static Packet deserialize(const std::string& data) {
        auto j = json::parse(data);
        Packet p;
        p.type = static_cast<PacketType>(j["type"].get<int>());
        p.payload = j["payload"];
        return p;
    }
};

#endif