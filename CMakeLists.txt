cmake_minimum_required(VERSION 3.10)
project(Dispute C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)

# Ajoutez le dossier GTK4 aux chemins d'inclusion
include_directories(${CMAKE_SOURCE_DIR}/client/src/GTK4)
include_directories(${GTK4_INCLUDE_DIRS})
link_directories(${GTK4_LIBRARY_DIRS})

add_executable(client 
    client/src/GTK4/main_ui.c
    client/src/GTK4/pages/connect_page.c
    client/src/GTK4/pages/register_page.c
    client/src/GTK4/pages/chat_friends_page.c
    client/src/GTK4/pages/chat_server_page.c 
)

target_link_libraries(client PRIVATE ${GTK4_LIBRARIES})

configure_file(
    ${CMAKE_SOURCE_DIR}/client/src/GTK4/model/profil.png
    ${CMAKE_BINARY_DIR}/client/src/GTK4/model/profil.png
    COPYONLY
)
configure_file(
    ${CMAKE_SOURCE_DIR}/client/src/GTK4/model/inbox.png
    ${CMAKE_BINARY_DIR}/client/src/GTK4/model/inbox.png
    COPYONLY
)
configure_file(
    ${CMAKE_SOURCE_DIR}/client/src/GTK4/model/add_friend_icon.png
    ${CMAKE_BINARY_DIR}/client/src/GTK4/model/add_friend_icon.png
    COPYONLY
)
configure_file(
    ${CMAKE_SOURCE_DIR}/client/src/GTK4/model/pas_content.png
    ${CMAKE_BINARY_DIR}/client/src/GTK4/model/pas_content.png
    COPYONLY
)