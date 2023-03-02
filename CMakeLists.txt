cmake_minimum_required(VERSION 3.15)
project(pomp VERSION 1.0)


list(APPEND LIBS pthread)

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

# checks if set up rpath exists for install
if(COMMAND set_up_rpath)
    set_up_rpath()
else()
    message("Set up rpath not defined!")
endif()

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
set_target_properties(${PROJECT_NAME} PROPERTIES
                    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
)

target_include_directories(${PROJECT_NAME} PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include>
)

# define the symbol stating we are using the declspec(dllexport) when
# building on windows
target_compile_definitions(${PROJECT_NAME} PRIVATE "POMP_API_EXPORTS")
target_compile_options(${PROJECT_NAME} PRIVATE "-fvisibility=hidden")

if(WIN32)
    list(APPEND LIBS ws2_32)
else()
    list(APPEND LIBS rt)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})

set(${PROJECT_NAME}-headers
    ${PROJECT_SOURCE_DIR}/include/
)

# state that lib${PROJECT_NAME} need PIC when the default is shared libraries
set_target_properties(${PROJECT_NAME} PROPERTIES
                     POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
)

install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# to get all header files correctly
# directly in include due to reasons
install(
    DIRECTORY ${${PROJECT_NAME}-headers} DESTINATION include/
)

