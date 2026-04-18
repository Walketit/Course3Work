/**
 * @file main.cpp
 * @brief Главный файл клиентской части.
 */
#include "common/Logger.h"

int main() {
    Logger::getInstance().log("Клиент запускается...");
    return 0;
}