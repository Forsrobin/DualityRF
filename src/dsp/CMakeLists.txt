add_library(duality_dsp STATIC
    Window.h
    Window.cpp
    FftEngine.h
    FftEngine.cpp
    SpectrumAccumulator.h
    SpectrumAccumulator.cpp
    SpectrumProcessor.h
    SpectrumProcessor.cpp
    WaveformGenerator.h
    WaveformGenerator.cpp
    Measurements.h
    Measurements.cpp
    PeakDetector.h
    PeakDetector.cpp
)

target_link_libraries(duality_dsp
    PUBLIC
        duality_core
    PRIVATE
        PkgConfig::FFTW
)
