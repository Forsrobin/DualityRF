// HackRF (libhackrf) baseband C16 replay similar to hackrf_transfer/PortaPack
#if __has_include(<libhackrf/hackrf.h>)
#  include <libhackrf/hackrf.h>
#elif __has_include(<hackrf.h>)
#  include <hackrf.h>
#else
#  error "libhackrf headers not found (install libhackrf-dev)"
#endif
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>

struct TxState {
  std::vector<int8_t> iq8; // interleaved I,Q signed 8-bit
  size_t pos{0};
};

static bool parse_config(const std::string &path, uint64_t &center_hz, uint32_t &sample_rate)
{
  center_hz = 0; sample_rate = 0;
  std::ifstream f(path);
  if (!f.is_open()) return false;
  std::string line;
  while (std::getline(f, line)) {
    auto p = line.find('=');
    if (p == std::string::npos) continue;
    auto k = line.substr(0, p);
    auto v = line.substr(p+1);
    try {
      if (k == "center_frequency") center_hz = static_cast<uint64_t>(std::stoll(v));
      else if (k == "sample_rate") sample_rate = static_cast<uint32_t>(std::stoul(v));
    } catch (...) {}
  }
  return center_hz > 0 && sample_rate > 0;
}

static int tx_callback(hackrf_transfer* transfer)
{
  TxState* st = reinterpret_cast<TxState*>(transfer->tx_ctx);
  const size_t need = transfer->valid_length; // bytes (I,Q interleaved int8)
  if (st->pos + need <= st->iq8.size()) {
    std::memcpy(transfer->buffer, st->iq8.data()+st->pos, need);
    st->pos += need;
  } else {
    // tail: send remaining then zeros
    size_t remain = (st->pos < st->iq8.size()) ? (st->iq8.size()-st->pos) : 0;
    if (remain > 0) std::memcpy(transfer->buffer, st->iq8.data()+st->pos, remain);
    if (need > remain) std::memset(transfer->buffer+remain, 0, need-remain);
    st->pos += need;
  }
  return 0;
}

int main(int argc, char** argv)
{
  std::string cfg = "BBD_0001.TXT";
  std::string dat = "BBD_0001.C16";
  if (argc >= 2) cfg = argv[1];
  if (argc >= 3) dat = argv[2];

  uint64_t center_hz; uint32_t samp_rate;
  if (!parse_config(cfg, center_hz, samp_rate)) {
    std::cerr << "[HACKRF] Failed to parse config: " << cfg << std::endl; return 1;
  }
  std::ifstream in(dat, std::ios::binary);
  if (!in.is_open()) { std::cerr << "[HACKRF] Failed to open data: " << dat << std::endl; return 1; }

  // Load all samples (C16 -> int8 with scaling)
  in.seekg(0, std::ios::end); std::streamsize bytes = in.tellg(); in.seekg(0, std::ios::beg);
  if (bytes <= 0) { std::cerr << "[HACKRF] Empty data" << std::endl; return 1; }
  size_t total_ns = size_t(bytes / (2*sizeof(int16_t)));
  std::vector<int16_t> c16(total_ns*2);
  if (!in.read(reinterpret_cast<char*>(c16.data()), bytes)) { std::cerr << "[HACKRF] Read error" << std::endl; return 1; }
  in.close();
  double peak = 0.0;
  for (size_t i=0;i<total_ns;++i) {
    double I = c16[2*i+0] / 32768.0; double Q = c16[2*i+1] / 32768.0; double m = std::sqrt(I*I+Q*Q); if (m>peak) peak=m;
  }
  double scale = (peak>0.0)? (0.95/peak) : 1.0; if (scale>8.0) scale=8.0;
  // Build buffer with pre/post roll of zeros (100ms each)
  size_t pre = size_t(samp_rate * 0.10);
  size_t post = size_t(samp_rate * 0.10);
  TxState st;
  st.iq8.resize((pre + total_ns + post)*2);
  size_t p=0;
  // pre zeros
  std::memset(st.iq8.data(), 0, pre*2);
  p = pre*2;
  // scaled samples to int8
  for (size_t i=0;i<total_ns;++i) {
    int i8 = int(std::lround((c16[2*i+0]/32768.0)*scale*127.0));
    int q8 = int(std::lround((c16[2*i+1]/32768.0)*scale*127.0));
    if (i8>127) i8=127; if (i8<-128) i8=-128; if (q8>127) q8=127; if (q8<-128) q8=-128;
    st.iq8[p++] = int8_t(i8);
    st.iq8[p++] = int8_t(q8);
  }
  // post zeros already zero-initialized because of resize

  // Setup HackRF
  if (hackrf_init() != HACKRF_SUCCESS) { std::cerr << "[HACKRF] init failed" << std::endl; return 2; }
  hackrf_device* dev = nullptr;
  if (hackrf_open(&dev) != HACKRF_SUCCESS || dev==nullptr) { std::cerr << "[HACKRF] open failed" << std::endl; hackrf_exit(); return 2; }

  hackrf_set_sample_rate(dev, samp_rate);
  // baseband filter to rounded supported bandwidth
  uint32_t bw = hackrf_compute_baseband_filter_bw_round_down_lt(samp_rate);
  if (bw < 1750000) bw = 1750000; // minimum practical
  hackrf_set_baseband_filter_bandwidth(dev, bw);
  hackrf_set_txvga_gain(dev, 15); // user request
  hackrf_set_amp_enable(dev, 1);
  hackrf_set_freq(dev, center_hz);

  // Start TX
  st.pos = 0;
  if (hackrf_start_tx(dev, tx_callback, &st) != HACKRF_SUCCESS) {
    std::cerr << "[HACKRF] start_tx failed" << std::endl; hackrf_close(dev); hackrf_exit(); return 3;
  }

  // Wait until we've sent all (rough wait); libhackrf pulls via callback
  double seconds = double(pre + total_ns + post) / double(samp_rate);
  const int ms = int(std::ceil((seconds + 0.2) * 1000)); // add guard
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));

  hackrf_stop_tx(dev);
  hackrf_close(dev);
  hackrf_exit();
  std::cerr << "[HACKRF] Replay done, seconds=" << seconds << " SR=" << samp_rate << std::endl;
  return 0;
}
