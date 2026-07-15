add_library(duality_sdr STATIC
    DeviceInfo.h
    ISDRDevice.h
    IReceiver.h
    ITransmitter.h
    DeviceManager.h
    DeviceManager.cpp
    soapy/SoapyDevice.h
    soapy/SoapyDevice.cpp
)

# SoapySDR's Config.hpp includes <ciso646>, which GCC flags under C++20.
target_compile_options(duality_sdr PRIVATE -Wno-cpp)

target_link_libraries(duality_sdr
    PUBLIC
        duality_core
    PRIVATE
        SoapySDR
)
