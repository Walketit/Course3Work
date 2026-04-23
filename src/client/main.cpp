/**
 * @file main.cpp
 * @brief Главный файл клиентской части с интерфейсом на ncurses.
 */
#include "client/Client.h"
#include "common/Packet.h"
#include <ncurses.h>
#include <iostream>
#include <string>
#include <atomic>
#include <vector>
#include <mutex>
#include <clocale>
#include <iomanip>
#include <ctime>
#include <cstdlib>

// Состояния клиента
enum class AppState {
    AUTH,       // Экран авторизации и регистрации
    MAIN_MENU,  // Главное меню + список чатов
    IN_CHAT     // Активное окно переписки
};

// Глобальное состояние интерфейса
std::atomic<AppState> currentState{AppState::AUTH};
std::atomic<int> myUserId{-1};
std::atomic<int> myChatId{-1};
std::string currentChatName = "";

std::mutex uiMutex;
std::vector<std::string> currentMessages;
std::atomic<bool> redrawNeeded{false};
std::string inputText = "";

// Переменная для скроллинга в окне чата
std::atomic<int> scrollOffset{0};

// Вспомогательная функция для безопасного удаления UTF-8 символов (Backspace)
void popBackUTF8(std::string& str) {
    if (str.empty()) return;
    if ((str.back() & 0xC0) == 0x80) {
        str.pop_back(); 
        if (!str.empty()) str.pop_back(); 
    } else {
        str.pop_back(); 
    }
}

// Получить текущее время в формате HH:MM
std::string getCurrentTimeShort() {
    std::time_t t = std::time(nullptr);
    char mbstr[10];
    if (std::strftime(mbstr, sizeof(mbstr), "%H:%M", std::localtime(&t))) {
        return std::string(mbstr);
    }
    return "--:--";
}

// Вытащить HH:MM из серверного формата YYYY-MM-DD HH:MM:SS
std::string extractTime(const std::string& datetime) {
    if (datetime.length() >= 16) {
        return datetime.substr(11, 5); 
    }
    return "--:--";
}

// Отрисовка окна истории переписки
void drawChatWindow(WINDOW* win) {
    wclear(win);
    box(win, 0, 0); 
    mvwprintw(win, 0, 2, " Чат: %s ", currentChatName.c_str());
    
    std::lock_guard<std::mutex> lock(uiMutex);
    int max_y, max_x;
    getmaxyx(win, max_y, max_x);
    
    int start_y = max_y - 2; 
    
    // Пропускаем сообщения в зависимости от скролла
    auto it = currentMessages.rbegin();
    for (int i = 0; i < scrollOffset && it != currentMessages.rend(); ++i) {
        ++it;
    }

    // Печатаем сообщения снизу вверх
    for (; it != currentMessages.rend() && start_y > 0; ++it) {
        mvwprintw(win, start_y--, 2, "%s", it->c_str());
    }

    // Если история пролистана, показываем подсказку
    if (scrollOffset > 0) {
        mvwprintw(win, max_y - 1, max_x - 15, " \xE2\x86\x93 Скролл "); 
    }

    // Применяем отрисовку к экрану
    wrefresh(win); 
}

// Окно ввода сообщений
void drawInputWindow(WINDOW* win) {
    wclear(win);
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " Ввод (Enter-отправить, /back-назад, \xE2\x86\x91\xE2\x86\x93 - листать чат колёсиком мышки) ");
    mvwprintw(win, 1, 2, "> %s", inputText.c_str());
    wrefresh(win);
}

// Ввод для статичных меню (авторизация)
std::string promptNcurses(const char* promptMessage) {
    printw("%s", promptMessage);
    refresh();
    char buffer[256];
    echo(); 
    getnstr(buffer, 255);
    noecho(); 
    return std::string(buffer);
}

int main() {
    // Настройка ncurses
    setlocale(LC_ALL, ""); 
    initscr(); // Инициализация графического буфера              
    cbreak(); // Отключаем строковую буферизацию              
    noecho(); // Отключаем авто-печать нажатых клавиш 
    keypad(stdscr, TRUE); // Разрешаем чтение спец-клавиши 
    curs_set(1); // Включаем видимость курсора           

    Client client;
    if (!client.connectToServer("127.0.0.1", 8080)) {
        endwin();
        std::cerr << "Не удалось подключиться к серверу.\n";
        return 1;
    }

    // Инструкция для фонового потока
    auto onNewMessage = [&](const Packet& pkt) {
        int chatId = pkt.payload["chat_id"];
        std::string text = pkt.payload["text"];

        if (currentState == AppState::IN_CHAT && chatId == myChatId) {
            std::lock_guard<std::mutex> lock(uiMutex);
            std::string timeStr = getCurrentTimeShort();
            currentMessages.push_back("[" + timeStr + "] " + currentChatName + ": " + text);
            // Флаг главному потоку на обновление окна
            redrawNeeded = true; 
        }
    };

    client.startListening(onNewMessage);
    
    // Главный цикл
    while (true) {
        clear();
        // Экран авторизации/регистрации
        if (currentState == AppState::AUTH) {
            printw("=== АВТОРИЗАЦИЯ ===\n");
            printw("1. Регистрация\n2. Вход\n3. Выход\n");

            // Запрашиваем действие и ждем Enter
            std::string choiceStr = promptNcurses("Выберите действие: ");
            if (choiceStr.empty()) continue;
            
            // Обработка выхода и защита от ввода неверных чисел
            int choice = choiceStr[0] - '0';
            if (choice == 3) break;
            if (choice != 1 && choice != 2) continue;

            std::string login = promptNcurses("Логин: ");
            std::string pass = promptNcurses("Пароль: ");
            
            // Формирование сетевого запроса к серверу
            Packet req;
            // Если choice == 1, тип REGISTER, иначе LOGIN
            req.type = (choice == 1) ? PacketType::REGISTER : PacketType::LOGIN;
            req.payload["username"] = login;
            req.payload["password"] = pass;
            // Сериализуем и отправляем по сокету
            client.sendData(req.serialize());
            // Ожидание ответа от сервера
            Packet resp = client.waitForResponse();

            // Защита от потери связи с сервером (пустой ответ с ошибкой)
            if (resp.type == PacketType::ERROR_RESPONSE && resp.payload.is_null()) break;

            // Вывод ответа
            clear();
            printw(">>> %s\n", resp.payload["message"].get<std::string>().c_str());
            printw("Нажмите любую клавишу для продолжения...");
            refresh();
            getch();

            // Смена состояния при успешной авторизации
            if (resp.type == PacketType::SUCCESS_RESPONSE && choice == 2) {
                myUserId = resp.payload["user_id"];
                currentState = AppState::MAIN_MENU;
            }
            // Если пользователь регистрировался, его вернёт обратно в меню,
            // для последующей авторизации.
        }
        // Главное меню мессенджера
        else if (currentState == AppState::MAIN_MENU) {
            printw("=== ГЛАВНОЕ МЕНЮ (Ваш ID: %d) ===\n", (int)myUserId);
            printw("1. Мои чаты\n2. Создать новый чат\n3. Выход\n");
            
            std::string choiceStr = promptNcurses("Выберите действие: ");
            if (choiceStr.empty()) continue;
            int choice = choiceStr[0] - '0';
            
            // Выход из аккаунта и возвращение в окно авторизации
            if (choice == 3) {
                myUserId = -1;
                currentState = AppState::AUTH;
                continue;
            }

            // Выбран пункт "Мои чаты"
            if (choice == 1) {
                // Формируем запрос на получение списка чатов
                Packet req;
                req.type = PacketType::GET_CHATS;
                req.payload["user_id"] = (int)myUserId;
                client.sendData(req.serialize());

                Packet resp = client.waitForResponse();
                if (resp.type == PacketType::CHAT_LIST_RESPONSE) {
                    auto chats = resp.payload["chats"]; // Кешируем массив чатов локально
                    bool chatSelected = false; // Флаг для выхода из локального цикла

                    // Внутренний цикл, пока не выберем чат или не нажмем 0 для выхода в меню
                    while (!chatSelected) {
                        clear();
                        printw("--- ВАШИ ЧАТЫ ---\n");
                        for (size_t i = 0; i < chats.size(); ++i) {
                            printw("[%zu] %s\n", i + 1, chats[i]["chat_name"].get<std::string>().c_str());
                        }
                        
                        std::string chatChoice = promptNcurses("Выберите чат (0 - отмена): ");
                        
                        try {
                            int idx = std::stoi(chatChoice);
                            
                            // Обработка кнопки "Отмена"
                            if (idx == 0) {
                                break;
                            }
                            // Если номер в правильном диапазоне
                            else if (idx > 0 && idx <= chats.size()) {
                                myChatId = chats[idx - 1]["chat_id"];
                                currentChatName = chats[idx - 1]["chat_name"];
                                // Смена состояния на окно выбранного чата
                                currentState = AppState::IN_CHAT;
                                chatSelected = true; // Успешно! Выходим из цикла
                            } 
                            // Иначе запрашиваем заново
                            else {
                                printw("\n>>> ОШИБКА: Чата с таким номером нет!\n");
                                printw("Нажмите любую клавишу...");
                                refresh();
                                getch();
                            }
                        } catch (...) {
                            // Если были введены не цифры
                            printw("\n>>> ОШИБКА: Пожалуйста, введите число!\n");
                            printw("Нажмите любую клавишу...");
                            refresh();
                            getch(); 
                        }
                    }
                }
            }
            // Создание нового чата
            else if (choice == 2) {
                std::string target = promptNcurses("Введите логин пользователя: ");
                // Формируем запрос на создание нового чата
                Packet req;
                req.type = PacketType::CREATE_CHAT;
                req.payload["sender_id"] = (int)myUserId;
                req.payload["target_username"] = target;
                client.sendData(req.serialize());

                Packet resp = client.waitForResponse();
                // Если чат создался возвращает ID нового чата/
                // либо если чат существовал, просто вернёт его ID
                if (resp.type == PacketType::SUCCESS_RESPONSE) {
                    myChatId = resp.payload["chat_id"]; // Запоминаем выданный сервером ID чата
                    currentChatName = target; // Название чата = имя собеседника
                    // Смена состояния на окно чата
                    currentState = AppState::IN_CHAT;
                // Сервер вернул ошибку (нет пользователя)
                } else {
                    printw(">>> ОШИБКА: %s\nНажмите любую клавишу...", resp.payload["message"].get<std::string>().c_str());
                    refresh();
                    getch();
                }
            }
        }
        // Окно активного чата (асинхронный режим)
        else if (currentState == AppState::IN_CHAT) {
            // Единожды запращиваем историю чата
            Packet reqHistory;
            reqHistory.type = PacketType::GET_CHAT_HISTORY;
            reqHistory.payload["chat_id"] = (int)myChatId;
            client.sendData(reqHistory.serialize());

            Packet respHistory = client.waitForResponse();
            {
                std::lock_guard<std::mutex> lock(uiMutex);
                currentMessages.clear();
                scrollOffset = 0; // Сбрасываем скролл при входе в чат
                
                // Заполняем массив сообщений текущего чата
                if (respHistory.type == PacketType::HISTORY_RESPONSE) {
                    for (const auto& msg : respHistory.payload["history"]) {
                        int sender = msg["sender_id"];
                        std::string author = (sender == myUserId) ? "Вы" : currentChatName;
                        std::string text = msg["content"];
                        std::string timeStr = extractTime(msg["timestamp"]);
                        currentMessages.push_back("[" + timeStr + "] " + author + ": " + text);
                    }
                }
            }
            // Создаём два окна, для отображения сообщений и их ввода
            // LINES и COLS - глобальные переменные ncurses, хранящие размер терминала
            WINDOW* chatWin = newwin(LINES - 3, COLS, 0, 0); // Занимает все, кроме нижних 3 строк
            WINDOW* inputWin = newwin(3, COLS, LINES - 3, 0); // Ровно 3 строки снизу
            
            // Задаём время ожидания ввода символов
            wtimeout(inputWin, 50); 
            keypad(inputWin, TRUE); // Включаем спец-клавиши для окна ввода
            
            inputText = "";
            redrawNeeded = true; // Первичная отрисовка окон
            
            // Цикл чата
            while (currentState == AppState::IN_CHAT) {
                // Отрисовка
                if (redrawNeeded) {
                    drawChatWindow(chatWin);
                    drawInputWindow(inputWin);
                    redrawNeeded = false;
                }

                int ch = wgetch(inputWin);
                
                // ERR значит, что за 50мс никто ничего не нажал
                if (ch != ERR) {
                    // Фильтр «Escape-последовательностей»
                    if (ch == 27) { 
                        nodelay(inputWin, TRUE);
                        while(wgetch(inputWin) != ERR) {}
                        wtimeout(inputWin, 50);
                        continue;
                    }

                    // Скроллинг чата вверх
                    if (ch == KEY_UP || ch == KEY_PPAGE) {
                        std::lock_guard<std::mutex> lock(uiMutex);
                        // Ограничиваем скролл, чтобы не уйти за пределы массива сообщений
                        if (scrollOffset < (int)currentMessages.size() - 1) {
                            scrollOffset++;
                            redrawNeeded = true;
                        }
                    } 
                    // Скроллинг чата вниз
                    else if (ch == KEY_DOWN || ch == KEY_NPAGE) {
                        std::lock_guard<std::mutex> lock(uiMutex);
                        if (scrollOffset > 0) {
                            scrollOffset--;
                            redrawNeeded = true;
                        }
                    }
                    // Отправка сообщения (Enter)
                    else if (ch == '\n') { 
                        // Если введена команда "/back" выходим в меню
                        if (inputText == "/back") {
                            currentState = AppState::MAIN_MENU;
                        } else if (!inputText.empty()) {
                            // Формируем пакет с сообщением
                            Packet reqSend;
                            reqSend.type = PacketType::SEND_MESSAGE;
                            reqSend.payload["sender_id"] = (int)myUserId;
                            reqSend.payload["chat_id"] = (int)myChatId;
                            reqSend.payload["text"] = inputText;
                            client.sendData(reqSend.serialize());
                            client.waitForResponse();
                            
                            {
                                std::lock_guard<std::mutex> lock(uiMutex);
                                std::string timeStr = getCurrentTimeShort();
                                currentMessages.push_back("[" + timeStr + "] Вы: " + inputText);
                                scrollOffset = 0; // При отправке скидываем скролл в самый низ
                            }
                            inputText = "";
                            redrawNeeded = true;
                        }
                    } 
                    // Обработка удаления
                    else if (ch == KEY_BACKSPACE) {
                        popBackUTF8(inputText);
                        redrawNeeded = true;
                    } 
                    // Обработка ввода текста
                    // 32 - пробел, всё что до 32 - системные символы
                    else if (ch >= 32 && ch < 256) { 
                        inputText += (char)ch;
                        redrawNeeded = true;
                    }
                }
            }
            // Освобождаем память терминала от окон при выходе из чата
            delwin(chatWin);
            delwin(inputWin);
        }
    }

    client.disconnect();
    endwin(); // Восстанавливает нормальную работу консоли
    return 0;
}