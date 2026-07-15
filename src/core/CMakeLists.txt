configure_file(Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/core/Version.h @ONLY)

add_library(duality_core INTERFACE
    Types.h
    RingBuffer.h
)

target_include_directories(duality_core INTERFACE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(duality_core INTERFACE Qt6::Core)
