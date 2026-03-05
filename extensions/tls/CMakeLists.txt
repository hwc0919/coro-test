set(NITROCORO_TLS_PROVIDER "openssl" CACHE STRING "TLS provider: openssl / botan")

add_library(nitrocoro-tls
    src/TlsStream.cc
)

target_include_directories(nitrocoro-tls
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(nitrocoro-tls
    PUBLIC
        nitrocoro
)

if(NITROCORO_TLS_PROVIDER STREQUAL "openssl")
    find_package(OpenSSL REQUIRED)
    target_sources(nitrocoro-tls PRIVATE src/providers/OpenSSLProvider.cc)
    target_link_libraries(nitrocoro-tls PRIVATE OpenSSL::SSL OpenSSL::Crypto)
elseif(NITROCORO_TLS_PROVIDER STREQUAL "botan")
    find_package(Botan REQUIRED)
    target_sources(nitrocoro-tls PRIVATE src/providers/BotanProvider.cc)
    target_link_libraries(nitrocoro-tls PRIVATE Botan::Botan)
else()
    message(FATAL_ERROR "Unknown NITROCORO_TLS_PROVIDER: ${NITROCORO_TLS_PROVIDER}")
endif()

include(GNUInstallDirs)

install(TARGETS nitrocoro-tls
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
    add_executable(tls_test tests/tls_test.cc)
    target_link_libraries(tls_test PRIVATE nitrocoro-tls)
    add_test(NAME tls_test COMMAND tls_test)
endif ()
