#include "dsp/FftEngine.h"

#include <fftw3.h>

#include <cassert>
#include <cstring>
#include <mutex>

namespace duality {

namespace {
std::mutex g_planMutex; // fftwf plan creation/destruction is not thread-safe
}

FftEngine::FftEngine(int size)
    : m_size(size)
{
    std::scoped_lock lock(g_planMutex);
    m_in = reinterpret_cast<Complex *>(fftwf_alloc_complex(size));
    m_out = reinterpret_cast<Complex *>(fftwf_alloc_complex(size));
    m_plan = fftwf_plan_dft_1d(size,
                               reinterpret_cast<fftwf_complex *>(m_in),
                               reinterpret_cast<fftwf_complex *>(m_out),
                               FFTW_FORWARD, FFTW_ESTIMATE);
}

FftEngine::~FftEngine()
{
    std::scoped_lock lock(g_planMutex);
    fftwf_destroy_plan(m_plan);
    fftwf_free(m_in);
    fftwf_free(m_out);
}

void FftEngine::execute(std::span<const Complex> in, std::span<Complex> out)
{
    assert(in.size() == static_cast<std::size_t>(m_size));
    assert(out.size() == static_cast<std::size_t>(m_size));
    std::memcpy(m_in, in.data(), m_size * sizeof(Complex));
    fftwf_execute(m_plan);
    std::memcpy(out.data(), m_out, m_size * sizeof(Complex));
}

} // namespace duality
