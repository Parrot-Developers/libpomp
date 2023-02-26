cmake_minimum_required(VERSION 3.15)
project(libpomp VERSION 1.0)

set(LIBS pthread)

set(LIB_SOURCES
    src/pomp_addr.c
    src/pomp_buffer.c
    src/pomp_conn.c
    src/pomp_ctx.c
    src/pomp_decoder.c
    src/pomp_encoder.c
    src/pomp_evt.c
    src/pomp_log.c
    src/pomp_loop.c
    src/pomp_msg.c
    src/pomp_prot.c
    src/pomp_watchdog.c
    src/pomp_timer.c
)

add_library(pomp ${LIB_SOURCES})

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
set_target_properties(pomp PROPERTIES
                    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
)

target_include_directories(pomp PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include>
)
# define the symbol stating we are using the declspec(dllexport) when
# building on windows
target_compile_definitions(pomp PRIVATE "POMP_API_EXPORTS")
target_compile_options(pomp PRIVATE "-fvisibility=hidden")

if(WIN32)
  target_link_libraries(pomp ws2_32)
endif()
target_link_libraries(pomp ${LIBS})