cmake_minimum_required(VERSION 4.1)
project(WBT)
set(CMAKE_CXX_STANDARD 26)

add_executable(WBT main.cpp
        winbintest/winbintest.hpp
        winbintest/winbintest.cpp
        vendor/nlohmann/json.hpp
        vendor/gunzip/gunzip.hpp)