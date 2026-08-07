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
    components/ProgramInfo.h
    components/ProgramInfo.cpp
    components/NumpadOverlay.h
    components/NumpadOverlay.cpp

    # Visualization widgets
    widgets/SpectrumWidget.h
    widgets/SpectrumWidget.cpp
    widgets/WaterfallWidget.h
    widgets/WaterfallWidget.cpp

    # Control panels
    panels/ICaptureControls.h
    panels/CapturePanel.h
    panels/CapturePanel.cpp
    panels/CaptureControlPanel.h
    panels/CaptureControlPanel.cpp
    panels/LookingPanel.h
    panels/LookingPanel.cpp
    panels/TransmitPanel.h
    panels/TransmitPanel.cpp
    panels/PlaybackPanel.h
    panels/PlaybackPanel.cpp
    panels/SessionList.h
    panels/SessionList.cpp
    panels/VizPanel.h
    panels/VizPanel.cpp

    # Program screens
    programs/FobProgram.h
    programs/FobProgram.cpp
    programs/CaptureProgram.h
    programs/CaptureProgram.cpp
    programs/SessionsProgram.h
    programs/SessionsProgram.cpp
    programs/LookingProgram.h
    programs/LookingProgram.cpp
    programs/DevicesProgram.h
    programs/DevicesProgram.cpp
    programs/JamProgram.h
    programs/JamProgram.cpp
    programs/BeaconProgram.h
    programs/BeaconProgram.cpp
    programs/SentryProgram.h
    programs/SentryProgram.cpp
    programs/RepeaterProgram.h
    programs/RepeaterProgram.cpp
    programs/IdentifierProgram.h
    programs/IdentifierProgram.cpp
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
