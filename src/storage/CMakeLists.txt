add_library(duality_storage STATIC
    RecordingMetadata.h
    RecordingMetadata.cpp
    IqFileWriter.h
    IqFileWriter.cpp
    IqFileReader.h
    IqFileReader.cpp
    FftCache.h
    FftCache.cpp
    Session.h
    Session.cpp
    SessionStore.h
    SessionStore.cpp
)

target_link_libraries(duality_storage
    PUBLIC
        duality_core
)
