cmake_minimum_required(VERSION 3.15)
project(pomp VERSION 1.0)

set(LIBS pthread rt)

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

add_library(${PROJECT_NAME} ${LIB_SOURCES})


option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

target_include_directories(${PROJECT_NAME} PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include>
)
# define the symbol stating we are using the declspec(dllexport) when
# building on windows
target_compile_definitions(${PROJECT_NAME} PRIVATE "POMP_API_EXPORTS")
target_compile_options(${PROJECT_NAME} PRIVATE "-fvisibility=hidden")

if(WIN32)
  target_link_libraries(${PROJECT_NAME} ws2_32)
endif()
target_link_libraries(${PROJECT_NAME} ${LIBS})

# state that lib${PROJECT_NAME} need PIC when the default is shared libraries
set_target_properties(${PROJECT_NAME} PROPERTIES
                     POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
)

install(TARGETS ${PROJECT_NAME}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${PROJECT_NAME}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${PROJECT_NAME}
)
