cmake_minimum_required(VERSION 3.10)
project(mon_app C)

set(CMAKE_C_STANDARD 11)

# Utilise pkg-config pour trouver GTK4
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)

# Inclure les headers GTK4
include_directories(${GTK4_INCLUDE_DIRS})
link_directories(${GTK4_LIBRARY_DIRS})
add_definitions(${GTK4_CFLAGS_OTHER})

# === Client ===
file(GLOB CLIENT_SOURCES "client/src/*.c")
add_executable(client ${CLIENT_SOURCES})
include_directories(client/include)
target_link_libraries(client PRIVATE ${GTK4_LIBRARIES})

# === Server ===
# file(GLOB SERVER_SOURCES "server/src/*.c")
# add_executable(server ${SERVER_SOURCES})
# include_directories(server/include)