cmake_minimum_required(VERSION 3.14)
project(AiAgentExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(OpenSSL REQUIRED)

add_executable(ai_agent
    src/main.cpp
    src/AiAgent.cpp
    src/Programming-Mentor.cpp
    utils/DB.cpp
)

target_include_directories(ai_agent 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${SQLITE3_INCLUDE_DIRS}
)

target_link_libraries(ai_agent
    PRIVATE
      nlohmann_json::nlohmann_json
      OpenSSL::SSL
      OpenSSL::Crypto
      ${SQLITE3_LIBRARIES}
)