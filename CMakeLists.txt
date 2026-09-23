# Die Lernapp fuer Sailfish OS -- ein Programm, drei Kurse.
#
# Welcher Kurs gebaut wird, entscheidet -DKURS=clehrer|segelschein|segelflug.
# Daraus folgen Paketname, Programmname, Kursdaten und Bilder; der Quelltext
# ist fuer alle drei derselbe. Die Harmattan-Fassung (Qt 4.7, QtQuick 1.1)
# steht unverandert in ~/ps/c-lehrer und wird daraus nicht gebaut.
cmake_minimum_required(VERSION 3.6)
project(harbour-lehrer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT DEFINED KURS)
    set(KURS clehrer)
endif()
set(APP harbour-${KURS})
set(SHAREDIR ${CMAKE_INSTALL_PREFIX}/share/${APP})
set(LIBEXECDIR ${CMAKE_INSTALL_PREFIX}/libexec/${APP})

find_package(Qt5 REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SAILFISHAPP REQUIRED sailfishapp)

# --- Die App ---------------------------------------------------------------

file(GLOB LEHRER_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
list(SORT LEHRER_SOURCES)

add_executable(${APP} ${LEHRER_SOURCES})

target_include_directories(${APP} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SAILFISHAPP_INCLUDE_DIRS}
)

target_compile_definitions(${APP} PRIVATE
    APP_NAME="${APP}"
    APP_SHARE_DIR="${SHAREDIR}"
    APP_LIBEXEC_DIR="${LIBEXECDIR}"
)

target_link_libraries(${APP}
    Qt5::Core Qt5::Gui Qt5::Qml Qt5::Quick ${SAILFISHAPP_LIBRARIES}
)

# --- crun: der C-Deuter ----------------------------------------------------
#
# picoc (New BSD), unveraendert bis auf das ausgebaute USE_READLINE -- der
# Schalter haengt oben an NO_FP, und Fliesskomma brauchen die Lektionen. Ein
# Deuter und kein Uebersetzer, damit eine Endlosschleife im ersten eigenen
# Programm einen Kindprozess kostet und nicht die App.

file(GLOB PICOC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/picoc/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/picoc/cstdlib/*.c
)
list(SORT PICOC_SOURCES)
list(APPEND PICOC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/picoc/platform/platform_unix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/picoc/platform/library_unix.c
)

add_executable(crun ${PICOC_SOURCES})
target_include_directories(crun PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/picoc)
target_compile_definitions(crun PRIVATE UNIX_HOST VER="2.1")
target_compile_options(crun PRIVATE -w -O2 -fomit-frame-pointer)
target_link_libraries(crun m)

# --- rrun: der Rust-Deuter -------------------------------------------------
#
# Eigenbau fuer genau den Ausschnitt, den der Kurs lehrt -- die Begruendung
# steht im Kopf von interp/rrun.cpp.

add_executable(rrun ${CMAKE_CURRENT_SOURCE_DIR}/interp/rrun.cpp)
target_compile_options(rrun PRIVATE -O2)
target_link_libraries(rrun m)

# --- Einbauen --------------------------------------------------------------

install(TARGETS ${APP} RUNTIME DESTINATION bin)
install(TARGETS crun rrun RUNTIME DESTINATION libexec/${APP})
install(DIRECTORY qml/ DESTINATION share/${APP}/qml)
install(FILES kurse/${KURS}/kurs.json DESTINATION share/${APP}/data)
install(FILES kurse/${KURS}/${APP}.desktop DESTINATION share/applications)

# Die Bilder gibt es nur bei den Kursen, die welche haben.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/kurse/${KURS}/bilder)
    install(DIRECTORY kurse/${KURS}/bilder/ DESTINATION share/${APP}/bilder)
endif()

foreach(GROESSE 86 108 128 172)
    install(FILES kurse/${KURS}/icons/${GROESSE}.png
            DESTINATION share/icons/hicolor/${GROESSE}x${GROESSE}/apps
            RENAME ${APP}.png)
endforeach()
