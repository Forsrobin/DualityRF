// Record from RTL-SDR via SoapySDR into C16 + TXT metadata
// Defaults: 3s at 433.81 MHz, 1.0 Msps
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>

static bool parseFlag(const std::string &a, const char *name, std::string &out)
{
  std::string key = std::string("--") + name + "=";
  if (a.rfind(key, 0) == 0) { out = a.substr(key.size()); return true; }
  return false;
}

int main(int argc, char** argv)
{
  // Defaults
  double centerHz = 433.81e6;
  double sampleRate = 1.0e6; // force 1000k as requested
  double durationSec = 3.0;
  std::string cfgPath = "BBD_0001.TXT";
  std::string datPath = "BBD_0001.C16";

  // Optional CLI flags:
  //   --freq=Hz --rate=Hz --sec=S --cfg=path --c16=path
  //   --gain=dB --agc --bw=Hz --offset=Hz --ppm=P --no-dcblock --device=kwargs
  bool useAgc = false;
  bool dcBlock = true;
  double gainDb = NAN;
  double bwHz = 0.0; // 0 = auto
  double loOffsetHz = 0.0; // 0 = direct tune
  double ppm = NAN;
  std::string deviceKwargs; // e.g. "driver=rtlsdr,serial=..."
  for (int i=1;i<argc;++i)
  {
    std::string a = argv[i];
    std::string v;
    if      (parseFlag(a, "freq", v)) centerHz = std::stod(v);
    else if (parseFlag(a, "rate", v)) sampleRate = std::stod(v);
    else if (parseFlag(a, "sec",  v)) durationSec = std::stod(v);
    else if (parseFlag(a, "cfg",  v)) cfgPath = v;
    else if (parseFlag(a, "c16",  v)) datPath = v;
    else if (parseFlag(a, "gain", v)) gainDb = std::stod(v);
    else if (parseFlag(a, "bw",   v)) bwHz = std::stod(v);
    else if (parseFlag(a, "offset", v)) loOffsetHz = std::stod(v);
    else if (parseFlag(a, "ppm",  v)) ppm = std::stod(v);
    else if (parseFlag(a, "device", v)) deviceKwargs = v;
    else if (a == "--agc") useAgc = true;
    else if (a == "--no-dcblock") dcBlock = false;
  }

  try {
    // Open RTL-SDR via SoapySDR
    SoapySDR::Kwargs args;
    if (!deviceKwargs.empty())
    {
      // Parse simple k=v[,k=v...] list into Kwargs
      size_t start = 0;
      while (start < deviceKwargs.size())
      {
        size_t comma = deviceKwargs.find(',', start);
        std::string pair = deviceKwargs.substr(start, comma == std::string::npos ? std::string::npos : (comma - start));
        size_t eq = pair.find('=');
        if (eq != std::string::npos)
          args[pair.substr(0, eq)] = pair.substr(eq+1);
        start = (comma == std::string::npos) ? deviceKwargs.size() : (comma + 1);
      }
    }
    if (args.count("driver") == 0) args["driver"] = "rtlsdr";
    std::cerr << "[REC] Opening via SoapySDR with args: ";
    for (const auto &kv : args) std::cerr << kv.first << "=" << kv.second << ",";
    std::cerr << std::endl;
    SoapySDR::Device* dev = SoapySDR::Device::make(args);
    if (!dev) { std::cerr << "[REC] No RTL-SDR found (Soapy driver=rtlsdr)" << std::endl; return 1; }

    // Configure RX: exact (request) sample rate
    dev->setSampleRate(SOAPY_SDR_RX, 0, sampleRate);
    // Optional RTL frequency correction (ppm)
    if (!std::isnan(ppm)) { try { dev->setFrequencyCorrection(SOAPY_SDR_RX, 0, ppm); } catch (...) {} }
    // Tune to center with optional LO OFFSET
    if (std::abs(loOffsetHz) > 0.0) {
      try { SoapySDR::Kwargs fargs; fargs["OFFSET"] = std::to_string((long long)loOffsetHz); dev->setFrequency(SOAPY_SDR_RX, 0, centerHz, fargs);} catch (...) { dev->setFrequency(SOAPY_SDR_RX, 0, centerHz);} }
    else { dev->setFrequency(SOAPY_SDR_RX, 0, centerHz); }
    // Optional bandwidth; 0 means leave automatic
    if (bwHz > 0.0) { try { dev->setBandwidth(SOAPY_SDR_RX, 0, bwHz); } catch (...) {} }
    // Query actual applied values (drivers may coerce)
    double actualRate = sampleRate;
    double actualFreq = centerHz;
    try { actualRate = dev->getSampleRate(SOAPY_SDR_RX, 0); } catch (...) {}
    try { actualFreq = dev->getFrequency(SOAPY_SDR_RX, 0); } catch (...) {}
    double actualBW = 0.0; try { actualBW = dev->getBandwidth(SOAPY_SDR_RX, 0); } catch (...) {}
    // Gain mode + device-specific AGC knob for RTL-SDR
    try { dev->setGainMode(SOAPY_SDR_RX, 0, useAgc); } catch (...) {}
    try { dev->writeSetting(SOAPY_SDR_RX, 0, "RTL_AGC", useAgc ? "true" : "false"); } catch (...) {}
    if (!useAgc)
    {
      double g = std::isnan(gainDb) ? 30.0 : gainDb;
      try {
        auto names = dev->listGains(SOAPY_SDR_RX, 0);
        if (std::find(names.begin(), names.end(), std::string("LNA")) != names.end())
          dev->setGain(SOAPY_SDR_RX, 0, "LNA", g);
        else if (std::find(names.begin(), names.end(), std::string("TUNER")) != names.end())
          dev->setGain(SOAPY_SDR_RX, 0, "TUNER", g);
        else dev->setGain(SOAPY_SDR_RX, 0, g);
      } catch (...) {}
    }

    // Try hardware/driver DC offset removal if available; keep software HPF as fallback
    try { dev->setDCOffsetMode(SOAPY_SDR_RX, 0, dcBlock); } catch (...) {}

    // Use interleaved 16-bit IQ to match C16 on disk
    SoapySDR::Stream* stream = dev->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CS16);
    if (!stream) { std::cerr << "[REC] setupStream failed" << std::endl; SoapySDR::Device::unmake(dev); return 2; }
    dev->activateStream(stream);
    std::cerr << "[REC] RX armed: f=" << actualFreq << " Hz sr=" << actualRate
              << " bw=" << actualBW << std::endl;

    // Settle: drain a short burst to flush startup transients
    {
      const size_t drainElems = size_t(actualRate * 0.10); // ~100ms
      const size_t drainChunk = std::min<size_t>(65536, drainElems);
      std::vector<int16_t> dBuf(drainChunk*2);
      size_t drained = 0; int dFlags=0; long long dTime=0;
      while (drained < drainElems)
      {
        void* dPtrs[1]; dPtrs[0] = dBuf.data();
        size_t n = std::min(drainChunk, drainElems - drained);
        int r = dev->readStream(stream, dPtrs, int(n), dFlags, dTime, 20000);
        if (r > 0) drained += size_t(r);
        else if (r == SOAPY_SDR_TIMEOUT) break;
        else std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    }

    // Prepare output
    std::ofstream dat(datPath, std::ios::binary);
    if (!dat.is_open()) { std::cerr << "[REC] Failed to open output: " << datPath << std::endl; dev->deactivateStream(stream); dev->closeStream(stream); SoapySDR::Device::unmake(dev); return 3; }

    // Use actual sample rate to determine total sample count for target duration
    const size_t totalSamples = static_cast<size_t>(actualRate * durationSec);
    size_t mtu = 0; try { mtu = dev->getStreamMTU(stream); } catch (...) { mtu = 0; }
    const size_t chunk = std::min<size_t>(65536, std::max<size_t>(4096, (mtu? mtu : 16384)));
    std::vector<int16_t> buf(chunk*2); // interleaved I,Q
    // Optional DC blocker (one-pole HPF)
    float prevXI = 0.0f, prevXQ = 0.0f, prevYI = 0.0f, prevYQ = 0.0f;
    const float r = 0.995f;

    size_t captured = 0;
    int flags = 0; long long timeNs = 0;
    while (captured < totalSamples)
    {
      void* buffs[1]; buffs[0] = buf.data();
      int toRead = int(std::min(chunk, totalSamples - captured));
      int ret = dev->readStream(stream, buffs, toRead, flags, timeNs, 1000000 /*1s*/);
      if (ret > 0)
      {
        if (dcBlock)
        {
          for (int i = 0; i < ret; ++i)
          {
            float xi = static_cast<float>(buf[2*i+0]);
            float xq = static_cast<float>(buf[2*i+1]);
            float yi = (xi - prevXI) + r*prevYI;
            float yq = (xq - prevXQ) + r*prevYQ;
            prevXI = xi; prevXQ = xq; prevYI = yi; prevYQ = yq;
            int vi = (int)std::lround(std::max(-32768.0f, std::min(32767.0f, yi)));
            int vq = (int)std::lround(std::max(-32768.0f, std::min(32767.0f, yq)));
            buf[2*i+0] = (int16_t)vi;
            buf[2*i+1] = (int16_t)vq;
          }
        }
        dat.write(reinterpret_cast<const char*>(buf.data()), size_t(ret)*2*sizeof(int16_t));
        captured += size_t(ret);
      }
      else if (ret == SOAPY_SDR_TIMEOUT)
      {
        std::cerr << "[REC] read timeout, continuing..." << std::endl;
      }
      else
      {
        std::cerr << "[REC] read error ret=" << ret << ", retrying..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
    dat.close();

    dev->deactivateStream(stream);
    dev->closeStream(stream);
    SoapySDR::Device::unmake(dev);

    // Write config TXT in the same format the replay tool expects
    std::ofstream cfg(cfgPath);
    if (cfg.is_open())
    {
      cfg << "center_frequency=" << static_cast<long long>(actualFreq) << "\n";
      cfg << "sample_rate=" << static_cast<long long>(actualRate) << "\n";
      cfg.close();
    }
    else
    {
      std::cerr << "[REC] Warning: failed to write config file: " << cfgPath << std::endl;
    }

    std::cerr << "[REC] Done. Captured samples=" << captured
              << " seconds=" << (double(captured)/actualRate) << std::endl;
    return 0;
  }
  catch (const std::exception &e)
  {
    std::cerr << "[REC] Exception: " << e.what() << std::endl;
    return 10;
  }
}
