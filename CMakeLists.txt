cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(GAME_MODEL_DIR "src/game_model")
set(JSON_LIB_DIR "src/json_lib")
set(REQUEST_HANDLER_DIR "src/request_handler")
set(HTTP_SERVER_DIR "src/http_server")
set(POSTGRES_DIR "src/postgres")

#----------------------------------------------------------
#                      LIBRARY
#----------------------------------------------------------

add_library(game_model STATIC
        ${GAME_MODEL_DIR}/model.h
        ${GAME_MODEL_DIR}/model.cpp
        ${GAME_MODEL_DIR}/loot_generator.h
        ${GAME_MODEL_DIR}/loot_generator.cpp
        ${GAME_MODEL_DIR}/tagged.h
        ${GAME_MODEL_DIR}/infrastructure.h
        ${GAME_MODEL_DIR}/geom.h
        ${GAME_MODEL_DIR}/collision_detector.h
        ${GAME_MODEL_DIR}/collision_detector.cpp
        ${GAME_MODEL_DIR}/model_serialization.h
)

#---------------------------------------------------------
#                      PROGRAM
#---------------------------------------------------------
add_executable(game_server
        src/main.cpp
        src/ticker.h
        src/sdk.h
        ${HTTP_SERVER_DIR}/http_server.cpp
        ${HTTP_SERVER_DIR}/http_server.h
        ${JSON_LIB_DIR}/boost_json.cpp
        ${JSON_LIB_DIR}/json_loader.h
        ${JSON_LIB_DIR}/json_loader.cpp
        ${REQUEST_HANDLER_DIR}/request_handler.cpp
        ${REQUEST_HANDLER_DIR}/request_handler.h
        ${REQUEST_HANDLER_DIR}/api_handler.h
        ${REQUEST_HANDLER_DIR}/api_handler.cpp
        ${REQUEST_HANDLER_DIR}/file_handler.h
        ${REQUEST_HANDLER_DIR}/file_handler.cpp
        ${POSTGRES_DIR}/postgres.h
        ${POSTGRES_DIR}/postgres.cpp
        ${POSTGRES_DIR}/connection_pool.h
)

target_compile_definitions(game_server PRIVATE BOOST_BEAST_USE_STD_STRING_VIEW)

target_include_directories(game_server PRIVATE src
        CONAN_PKG::boost
    )

target_link_libraries(game_server PUBLIC game_model
        Threads::Threads
        CONAN_PKG::boost
        CONAN_PKG::libpq
        CONAN_PKG::libpqxx
    )

#---------------------------------------------------------
#                         TESTS
#---------------------------------------------------------

find_package(Catch2 2 REQUIRED)

add_executable(loot_generator_tests
        tests/loot_generator_tests.cpp
        ${GAME_MODEL_DIR}/loot_generator.cpp
)

target_include_directories(loot_generator_tests PRIVATE ${GAME_MODEL_DIR})

target_link_libraries(loot_generator_tests PRIVATE
        CONAN_PKG::catch2
)

#---------------------------------------------------------

add_executable(collision_detector_tests
        tests/collision_detector_tests.cpp
        ${GAME_MODEL_DIR}/collision_detector.cpp
)

target_include_directories(collision_detector_tests PRIVATE ${GAME_MODEL_DIR})

target_link_libraries(collision_detector_tests PRIVATE
        CONAN_PKG::catch2
)

#---------------------------------------------------------

add_executable(state-serialization-tests
        tests/state-serialization-tests.cpp
        ${GAME_MODEL_DIR}/model_serialization.h
)

target_include_directories(state-serialization-tests PRIVATE
        src
        ${GAME_MODEL_DIR}
)

target_link_libraries(state-serialization-tests PRIVATE
        game_model
        CONAN_PKG::boost
        CONAN_PKG::catch2
)

