#include "common/Logger.h"
#include <chrono>
#include <iomanip>
#include <ctime>
#include <sstream>

// Реализация метода получения Singleton-объекта
Logger& Logger::getInstance() {
    // Статическая локальная переменная инициализируется только один раз.
    static Logger instance;
    return instance;
}

// В конструкторе открываем файл в режиме append (дозапись)
Logger::Logger() {
    logFile.open("logs/server.log", std::ios::app);
}

// При уничтожении объекта закрываем файл
Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

// Форматирование текущего времени
std::string Logger::getCurrentTimestamp() {
    // Получаем текущее время с точностью до системных часов
    auto now = std::chrono::system_clock::now();
    // Преобразуем в формат time_t
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    // std::put_time форматирует время по заданному шаблону
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// Конвертация перечисления в строку для вывода в текст
std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::DEBUG:   return "DEBUG";
        default:                return "UNKNOWN";
    }
}

// Главная функция логирования
void Logger::log(const std::string& message, LogLevel level) {
    // std::lock_guard автоматически захватывает мьютекс.
    // Если другой поток попытается вызвать log(), он уснет на этой строке
    // и будет ждать, пока первый поток не закончит запись и не выйдет из функции.
    std::lock_guard<std::mutex> lock(logMutex);
    
    // Формируем строку лога
    std::string formattedMessage = "[" + getCurrentTimestamp() + "] [" + 
                                   levelToString(level) + "] " + message;
    
    // Выводим в консоль
    std::cout << formattedMessage << std::endl;
    
    // Пишем в файл
    if (logFile.is_open()) {
        logFile << formattedMessage << std::endl;
    }
}