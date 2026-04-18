cmake_minimum_required(VERSION 3.10)
project(CourseWork)

# Стандарт C++17 для современных фич
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Пути к заголовочным файлам
include_directories(include)

# Создаем папку для логов
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/logs)

# Серверная часть
add_executable(messenger_server 
    src/server/main.cpp 
    src/common/Logger.cpp
)

# Клиентская часть
add_executable(messenger_client 
    src/client/main.cpp 
    src/common/Logger.cpp
)

# Для работы с потоками (pthread) на UNIX
find_package(Threads REQUIRED)
target_link_libraries(messenger_server Threads::Threads)
target_link_libraries(messenger_client Threads::Threads)