#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>
#include <complex>
#include <iostream>
#include <qt6/QtWidgets/QApplication>
#include <qt6/QtWidgets/QMainWindow>
#include <vector>

int main(int argc, char *argv[]) {
  try {
    // enumerate devices
    auto results = SoapySDR::Device::enumerate();
    std::cout << "Found " << results.size() << " device(s)" << std::endl;
    for (auto &r : results) {
      for (auto &it : r)
        std::cout << "  " << it.first << " = " << it.second << std::endl;
      std::cout << std::endl;
    }

    // open the first device (change "rtlsdr" -> "hackrf" if desired)
    SoapySDR::Kwargs args;
    args["driver"] = "rtlsdr";
    SoapySDR::Device *sdr = SoapySDR::Device::make(args);

    // set RX parameters
    sdr->setSampleRate(SOAPY_SDR_RX, 0, 2.4e6);
    sdr->setFrequency(SOAPY_SDR_RX, 0, 433.92e6);
    sdr->setGain(SOAPY_SDR_RX, 0, 40);

    std::cout << "Configured RX stream." << std::endl;

    // setup and ac:tivate stream
    SoapySDR::Stream *rxStream = sdr->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);
    sdr->activateStream(rxStream);

    // allocate buffer
    const size_t N = 4096;
    std::vector<std::complex<float>> buff(N);
    void *buffs[] = {buff.data()};
    // read samples
    int flags;
    long long timeNs;
    int ret = sdr->readStream(rxStream, buffs, N, flags, timeNs, 1e5);
    std::cout << "readStream returned " << ret << " samples" << std::endl;

    // show first few samples
    for (size_t i = 0; i < 5 && i < buff.size(); ++i)
      std::cout << i << ": " << buff[i] << std::endl;

    // cleanup
    sdr->deactivateStream(rxStream);
    sdr->closeStream(rxStream);
    SoapySDR::Device::unmake(sdr);
    std::cout << "Done." << std::endl;

    QApplication app(argc, argv);
    QMainWindow w;
    w.show();
    return app.exec();
  } catch (const std::exception &ex) {
    std::cerr << "Error: " << ex.what() << std::endl;
    return 1;
  }
}
