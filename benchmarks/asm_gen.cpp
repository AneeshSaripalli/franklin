// Assembly generation file for pipeline analysis
// This file instantiates the pipeline operations directly to generate clean assembly
#include "container/column.hpp"
#include "core/bf16.hpp"
#include <cstdint>

namespace franklin {

// Test the Int32Pipeline methods directly for clean assembly analysis
// This simulates the inner loop of vectorize() without template overhead

__attribute__((noinline))
void int32_pipeline_loop(const std::int32_t* __restrict__ fst_ptr,
                         const std::int32_t* __restrict__ snd_ptr,
                         std::int32_t* __restrict__ out_ptr,
                         std::size_t num_elements) {
    std::size_t offset = 0;
    const std::size_t step = Int32Pipeline::elements_per_iteration;

    while (offset + step <= num_elements) {
        // Load from memory
        auto a_storage = Int32Pipeline::load(fst_ptr + offset);
        auto b_storage = Int32Pipeline::load(snd_ptr + offset);

        // Transform to compute domain (no-op for int32)
        auto a_compute = Int32Pipeline::transform_to(a_storage);
        auto b_compute = Int32Pipeline::transform_to(b_storage);

        // Perform operation in compute domain
        auto result_compute = Int32Pipeline::op(a_compute, b_compute);

        // Transform back to storage domain (no-op for int32)
        auto result_storage = Int32Pipeline::transform_from(result_compute);

        // Store to memory
        Int32Pipeline::store(out_ptr + offset, result_storage);

        offset += step;
    }
}

// Test the BF16Pipeline methods directly for clean assembly analysis
__attribute__((noinline))
void bf16_pipeline_loop(const bf16* __restrict__ fst_ptr,
                        const bf16* __restrict__ snd_ptr,
                        bf16* __restrict__ out_ptr,
                        std::size_t num_elements) {
    std::size_t offset = 0;
    const std::size_t step = BF16Pipeline::elements_per_iteration;

    while (offset + step <= num_elements) {
        // Load from memory
        auto a_storage = BF16Pipeline::load(fst_ptr + offset);
        auto b_storage = BF16Pipeline::load(snd_ptr + offset);

        // Transform to compute domain (bf16 -> fp32)
        auto a_compute = BF16Pipeline::transform_to(a_storage);
        auto b_compute = BF16Pipeline::transform_to(b_storage);

        // Perform operation in compute domain
        auto result_compute = BF16Pipeline::op(a_compute, b_compute);

        // Transform back to storage domain (fp32 -> bf16)
        auto result_storage = BF16Pipeline::transform_from(result_compute);

        // Store to memory
        BF16Pipeline::store(out_ptr + offset, result_storage);

        offset += step;
    }
}

// Inline versions to see if compiler can optimize everything away
__attribute__((always_inline))
inline void int32_pipeline_loop_inline(const std::int32_t* __restrict__ fst_ptr,
                                       const std::int32_t* __restrict__ snd_ptr,
                                       std::int32_t* __restrict__ out_ptr,
                                       std::size_t num_elements) {
    std::size_t offset = 0;
    const std::size_t step = Int32Pipeline::elements_per_iteration;

    while (offset + step <= num_elements) {
        auto a_storage = Int32Pipeline::load(fst_ptr + offset);
        auto b_storage = Int32Pipeline::load(snd_ptr + offset);
        auto a_compute = Int32Pipeline::transform_to(a_storage);
        auto b_compute = Int32Pipeline::transform_to(b_storage);
        auto result_compute = Int32Pipeline::op(a_compute, b_compute);
        auto result_storage = Int32Pipeline::transform_from(result_compute);
        Int32Pipeline::store(out_ptr + offset, result_storage);
        offset += step;
    }
}

__attribute__((always_inline))
inline void bf16_pipeline_loop_inline(const bf16* __restrict__ fst_ptr,
                                      const bf16* __restrict__ snd_ptr,
                                      bf16* __restrict__ out_ptr,
                                      std::size_t num_elements) {
    std::size_t offset = 0;
    const std::size_t step = BF16Pipeline::elements_per_iteration;

    while (offset + step <= num_elements) {
        auto a_storage = BF16Pipeline::load(fst_ptr + offset);
        auto b_storage = BF16Pipeline::load(snd_ptr + offset);
        auto a_compute = BF16Pipeline::transform_to(a_storage);
        auto b_compute = BF16Pipeline::transform_to(b_storage);
        auto result_compute = BF16Pipeline::op(a_compute, b_compute);
        auto result_storage = BF16Pipeline::transform_from(result_compute);
        BF16Pipeline::store(out_ptr + offset, result_storage);
        offset += step;
    }
}

// Test wrappers that call the inline versions
__attribute__((noinline))
void test_int32_inline(const std::int32_t* fst, const std::int32_t* snd,
                       std::int32_t* out, std::size_t n) {
    int32_pipeline_loop_inline(fst, snd, out, n);
}

__attribute__((noinline))
void test_bf16_inline(const bf16* fst, const bf16* snd,
                      bf16* out, std::size_t n) {
    bf16_pipeline_loop_inline(fst, snd, out, n);
}

} // namespace franklin
