
add_executable(duality_rf main.cpp)

target_link_libraries(duality_rf PRIVATE SoapySDR)
target_link_libraries(duality_rf PRIVATE Qt6::Widgets)

target_include_directories(duality_rf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SoapySDR_INCLUDE_DIRS}
)
