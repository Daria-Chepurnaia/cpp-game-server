cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Debug)


include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(GameModel STATIC 
	src/model.cpp
	src/model.h
	src/loot.h
	src/loot_generator.h 
	src/loot_generator.cpp 
	src/tagged.h)

target_link_libraries(GameModel CONAN_PKG::boost)

add_executable(game_server
	src/main.cpp
	src/model_serialization.h 
	src/model_serialization.cpp 
	src/application.h 
	src/application.cpp
    src/logger.h
    src/logger.cpp
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
	src/boost_json.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/request_handler.cpp
	src/request_handler.h
	src/player.h
	src/player.cpp
	src/ticker.h
	src/db_manager.h 
	src/db_manager.cpp 
	src/geom.h 
	src/collision_detector.h 
	src/collision_detector.cpp
	src/data_structures.h
)

target_link_libraries(game_server PUBLIC CONAN_PKG::boost CONAN_PKG::libpqxx GameModel)

add_executable(game_server_tests
	tests/loot_generator_tests.cpp
	src/geom.h 
	src/collision_detector.h 
	src/collision_detector.cpp 
	tests/collision-detector-tests.cpp
)
target_link_libraries(game_server_tests PUBLIC CONAN_PKG::catch2 CONAN_PKG::boost Threads::Threads GameModel)



