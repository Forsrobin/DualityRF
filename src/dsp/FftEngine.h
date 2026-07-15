#pragma once

#include "core/Types.h"

#include <span>

struct fftwf_plan_s;

namespace duality {

// Thin RAII wrapper around one forward complex FFTW plan. Plan creation is
// globally serialized (FFTW requirement); execution is safe concurrently on
// distinct instances.
class FftEngine {
public:
    explicit FftEngine(int size);
    ~FftEngine();
    FftEngine(const FftEngine &) = delete;
    FftEngine &operator=(const FftEngine &) = delete;

    int size() const { return m_size; }

    void execute(std::span<const Complex> in, std::span<Complex> out);

private:
    int m_size;
    fftwf_plan_s *m_plan = nullptr;
    Complex *m_in = nullptr;
    Complex *m_out = nullptr;
};

} // namespace duality
