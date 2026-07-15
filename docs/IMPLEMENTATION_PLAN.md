# DualityRF — Incremental Implementation Plan

Each phase must configure and compile (`cmake --preset debug && cmake --build
--preset debug`) before the next begins.

| Phase | Deliverable | Contents |
|-------|-------------|----------|
| 1 | Build skeleton | Root + per-module CMake, C++20, `duality_core`: `Types.h` (StreamParams, Range, SampleFormat), `RingBuffer.h` (SPSC), `Version.h`; app stub links Qt |
| 2 | SDR layer | `ISDRDevice`, `IReceiver`, `ITransmitter`, `DeviceInfo`, `DeviceManager` (SoapySDR enumerate + classify), `SoapyDevice` adapter |
| 3 | DSP | `Window`, `FftEngine` (FFTW3f), `SpectrumProcessor` (worker QObject), `WaveformGenerator` (white/gaussian/CW/sine/file), `Measurements` (OBW, power) |
| 4 | Storage | `RecordingMetadata` (JSON), `Session`, `SessionStore`, `IqFileWriter`, `IqFileReader` (cs16/cf32, `.C16`+`.TXT` import), `FftCache` |
| 5 | Pipelines | `CapturePipeline` (monitor/record, duration, triggers), `PlaybackPipeline` (repeat, speed), `TransmitPipeline` (concurrent waveform TX) |
| 6 | UI | `Theme`, `SpectrumWidget`, `WaterfallWidget`, `DevicePanel`, `CapturePanel`, `TransmitPanel`, `PlaybackPanel`, `SessionBrowser`, `DebugWorkspace`, `MainWindow` |
| 7 | Integration | `main.cpp`, dock layout persistence, release build, README |

Out of scope for the first iteration (kept possible by the architecture):
plugin loading from shared objects (adapters are compiled in; the interface
boundary is already plugin-shaped), triggered capture on signal threshold,
GPU waterfall.
