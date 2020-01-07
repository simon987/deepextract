cmake_minimum_required(VERSION 3.7)
project(deepextract C)


set(CMAKE_C_STANDARD 99)
option(STATIC_BUILD "Static build" off)

add_executable(
        deepextract main.c
)

if (STATIC_BUILD)
    target_link_libraries(
            deepextract
            -static
            archive
            acl
    )
else ()
    target_link_libraries(
            deepextract
            archive
    )
endif ()
