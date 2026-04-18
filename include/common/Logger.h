#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>

/**
 * @brief Уровни логирования для классификации сообщений.
 */
enum class LogLevel {
    INFO,    // Информационные сообщения (обычная работа)
    WARNING, // Предупреждения (некритичные проблемы)
    ERROR,   // Ошибки (критичные сбои, обрывы связи)
    DEBUG    // Отладочная информация
};

/**
 * @brief Класс для логирования системных событий.
 * Реализует паттерн Singleton, гарантируя существование только одного 
 * экземпляра логгера на всю программу. Поддерживает потокобезопасную 
 * (thread-safe) запись в консоль и в файл.
 */
class Logger {
private:
    std::ofstream logFile; // Поток для записи в файл
    std::mutex logMutex;   // Мьютекс для защиты от гонки данных

    /**
     * @brief Конструктор логера.
     * Открывает файл для дозаписи.
     */
    Logger();

    /**
     * @brief Деструктор логера.
     * Закрывает файловый поток при завершении программы.
     */
    ~Logger();
    
    // Запрещаем копирование и присваивание объекта
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /**
     * @brief Преобразует enum LogLevel в строковое представление.
     */
    std::string levelToString(LogLevel level);

    /**
     * @brief Получает текущее системное время в удобном формате.
     * @return Строка с датой и временем (YYYY-MM-DD HH:MM:SS).
     */
    std::string getCurrentTimestamp();

public:
    /**
     * @brief Получить единственный экземпляр логгера.
     * @return Ссылка на объект Logger.
     */
    static Logger& getInstance();
    
    /**
     * @brief Записать сообщение в лог.
     * @param message Текст сообщения.
     * @param level Уровень важности сообщения (по умолчанию INFO).
     */
    void log(const std::string& message, LogLevel level = LogLevel::INFO);
};

#endif