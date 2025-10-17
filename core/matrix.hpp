#ifndef FRANKLIN_CORE_MATRIX_HPP
#define FRANKLIN_CORE_MATRIX_HPP

#include <cstdint>
#include <span>
#include <vector>

namespace franklin {

struct frame_descriptor {
  std::vector<void*> cols_;
  std::size_t col_length_;
};

template <typename Policy> class dynmat {
public:
  using value_type = typename Policy::value_type;

private:
  frame_descriptor descriptor_;

public:
  auto cols() const noexcept { return descriptor_.cols_.size(); }
  auto size() const noexcept { return descriptor_.col_length_; }
};

} // namespace franklin

#endif // FRANKLIN_CORE_MATRIX_HPP