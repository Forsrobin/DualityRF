#pragma once

#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

namespace duality {

// Lock-free single-producer/single-consumer ring buffer. The producer (RX
// worker) must never block, so write() drops data when full rather than
// waiting; the consumer polls with read().
template <typename T>
class SpscRingBuffer {
public:
    explicit SpscRingBuffer(std::size_t capacity)
        : m_buf(capacity + 1) // one slot kept empty to distinguish full/empty
    {
    }

    std::size_t write(std::span<const T> src)
    {
        const std::size_t head = m_head.load(std::memory_order_relaxed);
        const std::size_t tail = m_tail.load(std::memory_order_acquire);
        const std::size_t free = freeSpace(head, tail);
        const std::size_t n = std::min(src.size(), free);
        for (std::size_t i = 0; i < n; ++i)
            m_buf[(head + i) % m_buf.size()] = src[i];
        m_head.store((head + n) % m_buf.size(), std::memory_order_release);
        return n;
    }

    std::size_t read(std::span<T> dst)
    {
        const std::size_t tail = m_tail.load(std::memory_order_relaxed);
        const std::size_t head = m_head.load(std::memory_order_acquire);
        const std::size_t avail = used(head, tail);
        const std::size_t n = std::min(dst.size(), avail);
        for (std::size_t i = 0; i < n; ++i)
            dst[i] = m_buf[(tail + i) % m_buf.size()];
        m_tail.store((tail + n) % m_buf.size(), std::memory_order_release);
        return n;
    }

    std::size_t available() const
    {
        return used(m_head.load(std::memory_order_acquire),
                    m_tail.load(std::memory_order_acquire));
    }

    void clear() { m_tail.store(m_head.load()); }

private:
    std::size_t used(std::size_t head, std::size_t tail) const
    {
        return (head + m_buf.size() - tail) % m_buf.size();
    }
    std::size_t freeSpace(std::size_t head, std::size_t tail) const
    {
        return m_buf.size() - 1 - used(head, tail);
    }

    std::vector<T> m_buf;
    std::atomic<std::size_t> m_head{0};
    std::atomic<std::size_t> m_tail{0};
};

} // namespace duality
