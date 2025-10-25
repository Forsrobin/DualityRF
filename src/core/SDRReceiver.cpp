
#include "SDRReceiver.h"
#include <QTimer>
#include <QVector>
#include <SoapySDR/Formats.h>
#include <cmath>
#include <complex>
#include <fftw3.h>

SDRReceiver::SDRReceiver(QObject *parent)
    : QObject(parent), running(false), freq(433.92), rate(2.4e6) {}

SDRReceiver::~SDRReceiver() { stop(); }

void SDRReceiver::start(double freqMHz, double sampleRate) {
  if (running)
    return;
  freq = freqMHz * 1e6;
  rate = sampleRate;
  running = true;
  moveToThread(&worker);
  connect(&worker, &QThread::started, this, &SDRReceiver::run);
  worker.start();
}

void SDRReceiver::stop() {
  if (!running)
    return;
  running = false;
  worker.quit();
  worker.wait();
}

void SDRReceiver::run() {
  try {
    SoapySDR::Kwargs args;
    args["driver"] = "rtlsdr";
    auto *dev = SoapySDR::Device::make(args);
    dev->setSampleRate(SOAPY_SDR_RX, 0, rate);
    dev->setFrequency(SOAPY_SDR_RX, 0, freq);
    dev->setGain(SOAPY_SDR_RX, 0, 40);

    auto *stream = dev->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);
    dev->activateStream(stream);

    const size_t N = 2048;
    std::vector<std::complex<float>> buff(N);
    void *buffs[] = {buff.data()};

    fftwf_complex *in =
        (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * N);
    fftwf_complex *out =
        (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * N);
    fftwf_plan plan =
        fftwf_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    while (running) {
      int flags;
      long long timeNs;
      int ret = dev->readStream(stream, buffs, N, flags, timeNs, 1e5);
      if (ret <= 0)
        continue;

      for (int i = 0; i < ret; ++i) {
        in[i][0] = buff[i].real();
        in[i][1] = buff[i].imag();
      }

      fftwf_execute(plan);

      QVector<float> magnitudes(N);
      for (int i = 0; i < N; ++i) {
        float re = out[i][0], im = out[i][1];
        magnitudes[i] = std::sqrt(re * re + im * im);
      }
      emit newFFTData(magnitudes);
    }

    fftwf_destroy_plan(plan);
    fftwf_free(in);
    fftwf_free(out);

    dev->deactivateStream(stream);
    dev->closeStream(stream);
    SoapySDR::Device::unmake(dev);
  } catch (...) {
    running = false;
  }
}
