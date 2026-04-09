add_library(nitrocoro-wss STATIC
    src/WssClient.cc
)

target_include_directories(nitrocoro-wss
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(nitrocoro-wss
    PUBLIC
        nitrocoro-websocket
        nitrocoro-tls
)

include(GNUInstallDirs)

install(TARGETS nitrocoro-wss
    EXPORT nitrocoroTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/nitrocoro
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

option(BUILD_TESTING "Build tests" ON)
if (BUILD_TESTING)
    enable_testing()
    find_package(OpenSSL REQUIRED)
    add_executable(wss_test tests/wss_test.cc)
    target_link_libraries(wss_test PRIVATE nitrocoro-wss OpenSSL::SSL OpenSSL::Crypto)
    add_test(NAME wss_test COMMAND wss_test)
endif ()
