/**
 * @file main.cpp
 * @brief Главный файл клиентской части.
 */
#include "common/Logger.h"
#include "client/Client.h"
#include "common/Packet.h"
#include <iostream>
#include <string>

int main() {
    Client client;

    Logger::getInstance().log("Клиент запускается...");

    // Подключаемся
    if (!client.connectToServer("127.0.0.1", 8080)) {
        return 1;
    }

    // Читаем приветственное сообщение от сервера
    std::string welcome = client.receiveData();
    std::cout << "\n--- Сообщение от сервера ---\n" << welcome << "--------------------------\n";

    // Имитируем отправку JSON пакета
    std::cout << "Формируем JSON пакет для отправки..." << std::endl;
    
    Packet sendPacket;
    sendPacket.type = PacketType::SEND_MESSAGE;
    sendPacket.payload["sender_id"] = 1;
    sendPacket.payload["chat_id"] = 42;
    sendPacket.payload["text"] = "Привет, сервер! Это настоящий C++ клиент!";

    // Сериализуем и отправляем
    std::string jsonString = sendPacket.serialize();
    std::cout << "Отправляем данные: " << jsonString << std::endl;
    
    client.sendData(jsonString);

    // Ждем ответа перед закрытием
    std::string response = client.receiveData();
    Packet responsePacket = Packet::deserialize(response);
    std::cout << "Ответ сервера: " << responsePacket.payload["info"] << std::endl;

    client.disconnect();
    return 0;
}