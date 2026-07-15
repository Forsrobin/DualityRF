add_library(duality_pipeline STATIC
    CapturePipeline.h
    CapturePipeline.cpp
    PlaybackPipeline.h
    PlaybackPipeline.cpp
    TransmitPipeline.h
    TransmitPipeline.cpp
)

target_link_libraries(duality_pipeline
    PUBLIC
        duality_core
        duality_sdr
        duality_dsp
        duality_storage
)
