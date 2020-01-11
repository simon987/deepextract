cmake_minimum_required(VERSION 3.7)
project(deepextract C)


set(CMAKE_C_STANDARD 11)
option(STATIC_BUILD "Static build" off)

add_executable(
        deepextract
        main.c
        thirdparty/argparse/argparse.c
        thirdparty/argparse/argparse.h
        extract.c extract.h
        tpool.c tpool.h
)

if (STATIC_BUILD)
    target_link_libraries(
            deepextract
            -static
            archive
            acl
            pthread

            lzma
            lz4
            zstd
            bz2
            z
            crypto
    )
else ()
    target_link_libraries(
            deepextract
            archive
            pthread
    )
endif ()

target_compile_options(
        deepextract
        PRIVATE
        -Ofast
        -fno-stack-protector
        -fomit-frame-pointer
)

