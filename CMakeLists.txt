cmake_minimum_required(VERSION 3.0.0)
project(blockchain VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)

add_executable(blockchain src/main.cpp)
target_sources(blockchain PRIVATE src/utils.cpp)


set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

include_directories(/usr/include)
link_directories(/usr/lib)

find_package(CryptoPP)

target_link_libraries(blockchain cryptopp)

include(CPack)



