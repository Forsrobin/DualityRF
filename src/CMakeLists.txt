
add_executable(duality_rf
    main.cpp
    ui/SplashScreen.cpp
    ui/MainWindow.cpp
    ui/WaterfallWidget.cpp
    ui/CapturePreviewWidget.cpp
    ui/WaveformWidget.cpp
    ui/SpectrumWidget.cpp
    ui/componenets/InfoDialog.cpp
    core/SDRManager.cpp
    core/SDRReceiver.cpp
    core/SDRTransmitter.cpp
    resources.qrc
)

target_link_libraries(duality_rf
    PRIVATE
        SoapySDR
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        ${FFTW_LIBRARIES}
)

target_include_directories(duality_rf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${SoapySDR_INCLUDE_DIRS}
    ${FFTW_INCLUDE_DIRS}
)

add_executable(record_hackrf test/record_hackrf.cpp)
target_link_libraries(record_hackrf PRIVATE SoapySDR)
add_executable(replay_hackrf test/replay_hackrf.cpp)
target_link_libraries(replay_hackrf PRIVATE hackrf)
