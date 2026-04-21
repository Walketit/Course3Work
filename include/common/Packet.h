/**
 * @file Packet.h
 * @brief Структура сетевого пакета для обмена между клиентом и сервером.
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
    REGISTER,
    LOGIN,
    CREATE_CHAT,
    SEND_MESSAGE,
    ERROR_RESPONSE,
    SUCCESS_RESPONSE
};

/**
 * @brief Класс, представляющий единицу данных, передаваемую по сети.
 */
struct Packet {
    PacketType type;
    json payload; // Полезная нагрузка (любые данные в формате JSON)

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