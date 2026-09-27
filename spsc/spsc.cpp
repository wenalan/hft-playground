#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <thread>
#include <utility>

template <typename T, std::size_t N>
class spsc_ring_buffer {
    static_assert(N > 0 && (N & (N - 1)) == 0);

    static constexpr std::size_t cacheline = 64;

    alignas(cacheline)
    std::atomic<std::size_t> producer_counter_{0};

    alignas(cacheline)
    std::atomic<std::size_t> consumer_counter_{0};

    alignas(cacheline)
    std::array<T, N> buffer_{};

public:
    bool try_enqueue(T value)
    {
        const auto p =
            producer_counter_.load(std::memory_order_relaxed);

        const auto c =
            consumer_counter_.load(std::memory_order_acquire);

        if (p - c >= N)
            return false;

        buffer_[p & (N - 1)] = std::move(value);

        producer_counter_.store(
            p + 1,
            std::memory_order_release);

        return true;
    }

    bool try_dequeue(T& value)
    {
        const auto c =
            consumer_counter_.load(std::memory_order_relaxed);

        const auto p =
            producer_counter_.load(std::memory_order_acquire);

        if (p == c)
            return false;

        value = std::move(buffer_[c & (N - 1)]);

        consumer_counter_.store(
            c + 1,
            std::memory_order_release);

        return true;
    }
};


struct Message {
    std::uint64_t sequence;
    std::uint64_t a;
    std::uint64_t b;
    std::uint64_t c;
};

static constexpr std::uint64_t MAGIC_A =
    0x123456789abcdef0ULL;

static constexpr std::uint64_t MAGIC_B =
    0xfedcba9876543210ULL;

static constexpr std::uint64_t MAGIC_C =
    0xdeadbeefcafebabeULL;


void stress_test()
{
    constexpr std::size_t QUEUE_SIZE = 1024;
    constexpr std::uint64_t COUNT = 50'000'000;

    spsc_ring_buffer<Message, QUEUE_SIZE> queue;

    std::atomic<bool> failed{false};

    std::thread producer([&] {
        for (std::uint64_t i = 0; i < COUNT; ++i) {

            Message m{
                .sequence = i,
                .a = i ^ MAGIC_A,
                .b = i ^ MAGIC_B,
                .c = i ^ MAGIC_C,
            };

            while (!queue.try_enqueue(m)) {
                // deliberately busy spin
                std::this_thread::yield();
            }

            // Introduce some timing variation.
            if ((i & 0xffff) == 0)
                std::this_thread::yield();
        }
    });


    std::thread consumer([&] {
        for (std::uint64_t expected = 0;
             expected < COUNT;
             ++expected) {

            Message m{};

            while (!queue.try_dequeue(m)) {
                std::this_thread::yield();
            }

            if (m.sequence != expected) {
                std::cerr
                    << "sequence mismatch: expected="
                    << expected
                    << " got="
                    << m.sequence
                    << '\n';

                failed.store(true,
                             std::memory_order_relaxed);
                return;
            }

            if (m.a != (m.sequence ^ MAGIC_A) ||
                m.b != (m.sequence ^ MAGIC_B) ||
                m.c != (m.sequence ^ MAGIC_C)) {

                std::cerr
                    << "corrupted message at sequence "
                    << m.sequence
                    << '\n';

                failed.store(true,
                             std::memory_order_relaxed);
                return;
            }

            if ((expected & 0xffff) == 0)
                std::this_thread::yield();
        }
    });

    producer.join();
    consumer.join();

    assert(!failed.load());

    std::cout << "stress test passed\n";
}


void full_empty_test()
{
    spsc_ring_buffer<int, 4> q;

    int value;

    assert(!q.try_dequeue(value));

    assert(q.try_enqueue(1));
    assert(q.try_enqueue(2));
    assert(q.try_enqueue(3));
    assert(q.try_enqueue(4));

    assert(!q.try_enqueue(5));

    assert(q.try_dequeue(value));
    assert(value == 1);

    assert(q.try_dequeue(value));
    assert(value == 2);

    assert(q.try_enqueue(5));
    assert(q.try_enqueue(6));

    assert(q.try_dequeue(value));
    assert(value == 3);

    assert(q.try_dequeue(value));
    assert(value == 4);

    assert(q.try_dequeue(value));
    assert(value == 5);

    assert(q.try_dequeue(value));
    assert(value == 6);

    assert(!q.try_dequeue(value));

    std::cout << "full/empty test passed\n";
}


int main()
{
    full_empty_test();
    stress_test();
}

