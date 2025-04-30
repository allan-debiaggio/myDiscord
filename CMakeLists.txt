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
# Ajoutez explicitement main_ui.c à la liste des sources
set(CLIENT_SOURCES 
    client/src/GTK4/main_ui.c
)

# Créez l'exécutable
add_executable(client ${CLIENT_SOURCES})

# Configuration des ressources (style.css)
configure_file(
    ${CMAKE_SOURCE_DIR}/client/src/GTK4/style.css
    ${CMAKE_BINARY_DIR}/client/src/GTK4/style.css
    COPYONLY
)

# Inclure les répertoires d'en-têtes
include_directories(client/include)

# Lier avec GTK4
target_link_libraries(client PRIVATE ${GTK4_LIBRARIES})

# === Server ===
# file(GLOB SERVER_SOURCES "server/src/*.c")
# add_executable(server ${SERVER_SOURCES})
# include_directories(server/include)