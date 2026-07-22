add_library(duality_ui STATIC
    # App + screens (main ui folder)
    Theme.h
    Theme.cpp
    HomePage.h
    HomePage.cpp
    ProgramScreen.h
    ProgramScreen.cpp
    SplashPage.h
    SplashPage.cpp
    MainWindow.h
    MainWindow.cpp

    # Reusable components
    components/FlowLayout.h
    components/FlowLayout.cpp
    components/HazardButton.h
    components/HazardButton.cpp
    components/ToastManager.h
    components/ToastManager.cpp
    components/DeviceBadge.h
    components/DeviceBadge.cpp
    components/ScanIndicator.h
    components/ScanIndicator.cpp

    # Visualization widgets
    widgets/SpectrumWidget.h
    widgets/SpectrumWidget.cpp
    widgets/WaterfallWidget.h
    widgets/WaterfallWidget.cpp

    # Control panels
    panels/CapturePanel.h
    panels/CapturePanel.cpp
    panels/TransmitPanel.h
    panels/TransmitPanel.cpp
    panels/PlaybackPanel.h
    panels/PlaybackPanel.cpp
    panels/SessionBrowser.h
    panels/SessionBrowser.cpp
    panels/VizPanel.h
    panels/VizPanel.cpp

    # Program screens
    programs/FobProgram.h
    programs/FobProgram.cpp
    programs/DevicesProgram.h
    programs/DevicesProgram.cpp
    programs/JamProgram.h
    programs/JamProgram.cpp
    programs/DebugProgram.h
    programs/DebugProgram.cpp
    programs/InfoProgram.h
    programs/InfoProgram.cpp
)

target_link_libraries(duality_ui
    PUBLIC
        duality_core
        duality_sdr
        duality_dsp
        duality_storage
        duality_pipeline
        Qt6::Widgets
)
