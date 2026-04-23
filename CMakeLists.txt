cmake_minimum_required(VERSION 3.10)
project(CourseWork)

# Стандарт C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Отладка для проверки на утечку памяти
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

# Пути к заголовочным файлам
include_directories(include)

# Создаем папку для логов
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/logs)

# Поиск библиотеки SQLite3
find_package(SQLite3 REQUIRED)

# Для работы с потоками
find_package(Threads REQUIRED)

# Серверная часть
add_executable(messenger_server 
    src/server/main.cpp 
    src/common/Logger.cpp
    src/server/DatabaseManager.cpp
    src/server/Server.cpp
)

# Клиентская часть
add_executable(messenger_client 
    src/client/main.cpp 
    src/common/Logger.cpp
    src/client/Client.cpp
)

target_link_libraries(messenger_server Threads::Threads)
target_link_libraries(messenger_server SQLite::SQLite3)
target_link_libraries(messenger_client Threads::Threads)
