#pragma once

#include <include/non_movable.h>
#include <include/non_copyable.h>

#include <vector>
#include <atomic>
#include <cstdint>
#include <type_traits>


namespace bcpp
{

    template <typename T, std::uint64_t N>
    class alignas(64) mpsc_queue :
        non_movable,
        non_copyable
    {
    public:


        mpsc_queue();

        template <typename T_>
        bool push
        (
            T_ &&
        );

        std::pair<bool, std::uint64_t> pop
        (
            T &
        );

        bool empty() const noexcept;

    private:

        static auto constexpr capacity = (1 << (64 - std::countl_zero(N) - 1));
        static auto constexpr capacity_mask = (capacity - 1);

        static std::uint64_t constexpr busy_flag     = 0x4000000000000000;
        static std::uint64_t constexpr ready_flag    = 0x8000000000000000;

        std::array<std::atomic<std::uint64_t>, capacity>    index_;

        std::array<T, capacity>                             data_;

        std::atomic<std::uint64_t>                          writeIndex_{0};

        std::atomic<std::uint64_t>                          readIndex_{0};
    };

} // namespace bcpp


//=============================================================================
template <typename T, std::uint64_t N>
inline bcpp::mpsc_queue<T, N>::mpsc_queue
(
)
{
}


//=============================================================================
template <typename T, std::uint64_t N>
template <typename T_>
inline bool bcpp::mpsc_queue<T, N>::push
(
    T_ && data
)
{
    auto writeIndex = writeIndex_.load();
    auto expected = (writeIndex < capacity) ? 0 : writeIndex;
    auto desired = (expected | busy_flag);
    auto maskedWriteIndex = writeIndex & capacity_mask;
    if (!index_[maskedWriteIndex].compare_exchange_strong(expected, desired))
        return false; // slot is being written into or filled with unconsumed value
    ++writeIndex_;
    if constexpr (std::is_trivially_copyable_v<T>)
    {
        data_[maskedWriteIndex] = std::forward<T_>(data);
    }
    else
    {
        new (&data_[maskedWriteIndex]) T(std::forward<T_>(data));
    }
    index_[maskedWriteIndex] ^= (busy_flag | ready_flag);
    return true; 
}


//=============================================================================
template <typename T, std::uint64_t N>
inline std::pair<bool, std::uint64_t> bcpp::mpsc_queue<T, N>::pop
(
    T & data
)
{
    auto readIndex = readIndex_.load();
    auto maskedReadIndex = readIndex & capacity_mask;
    if (index_[maskedReadIndex] < ready_flag)
        return {false, 0};
    data = data_[maskedReadIndex];
    index_[maskedReadIndex] = (readIndex + capacity);
    readIndex_ = readIndex + 1;
    return {true, writeIndex_ - readIndex};
}


//=============================================================================
template <typename T, std::uint64_t N>
inline bool bcpp::mpsc_queue<T, N>::empty
(
) const noexcept
{
    return (readIndex_ >= writeIndex_);
}
