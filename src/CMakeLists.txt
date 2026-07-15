add_subdirectory(core)
add_subdirectory(sdr)
add_subdirectory(dsp)
add_subdirectory(storage)
add_subdirectory(pipeline)
add_subdirectory(ui)

add_executable(duality_rf
    main.cpp
    resources.qrc
)

target_link_libraries(duality_rf
    PRIVATE
        duality_ui
)
