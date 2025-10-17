#include "python/franklin_c_api.h"
#include "container/column.hpp"
#include "core/interpreter.hpp"
#include <memory>
#include <stdexcept>
#include <string>

namespace franklin {

// Use default policies
using Int32Policy = franklin::Int32DefaultPolicy;
using Float32Policy = franklin::Float32DefaultPolicy;

// Type-erased column wrapper - stores the policy type and pointer to column
struct FranklinColumnImpl {
  DataTypeEnum::Enum policy_id;
  void* col_ptr; // Pointer to column_vector<Policy>

  template <typename Policy>
  explicit FranklinColumnImpl(column_vector<Policy>* col)
      : policy_id(Policy::policy_id), col_ptr(col) {}

  ~FranklinColumnImpl() {
    switch (policy_id) {
    case DataTypeEnum::Int32Default:
      delete static_cast<column_vector<Int32Policy>*>(col_ptr);
      break;
    case DataTypeEnum::Float32Default:
      delete static_cast<column_vector<Float32Policy>*>(col_ptr);
      break;
    case DataTypeEnum::BF16Default:
      delete static_cast<column_vector<BF16DefaultPolicy>*>(col_ptr);
      break;
    default:
      break;
    }
  }

  // Get typed pointer
  template <typename Policy> column_vector<Policy>* get_as() {
    if (policy_id != Policy::policy_id) {
      throw std::runtime_error("Type mismatch in get_as");
    }
    return static_cast<column_vector<Policy>*>(col_ptr);
  }

  size_t get_size() const {
    switch (policy_id) {
    case DataTypeEnum::Int32Default:
      return static_cast<column_vector<Int32Policy>*>(col_ptr)->data().size();
    case DataTypeEnum::Float32Default:
      return static_cast<column_vector<Float32Policy>*>(col_ptr)->data().size();
    case DataTypeEnum::BF16Default:
      return static_cast<column_vector<BF16DefaultPolicy>*>(col_ptr)
          ->data()
          .size();
    default:
      return 0;
    }
  }

  int32_t get_int32(size_t index) const {
    if (policy_id != DataTypeEnum::Int32Default) {
      throw std::runtime_error("Column is not int32");
    }
    auto* col = static_cast<column_vector<Int32Policy>*>(col_ptr);
    if (index >= col->data().size()) {
      throw std::out_of_range("Index out of bounds");
    }
    return col->data()[index];
  }

  float get_float32(size_t index) const {
    if (policy_id != DataTypeEnum::Float32Default) {
      throw std::runtime_error("Column is not float32");
    }
    auto* col = static_cast<column_vector<Float32Policy>*>(col_ptr);
    if (index >= col->data().size()) {
      throw std::out_of_range("Index out of bounds");
    }
    return col->data()[index];
  }
};

// Interpreter wrapper - now just wraps the non-templated interpreter
struct FranklinInterpreterImpl {
  interpreter interp;

  FranklinInterpreterImpl() = default;

  ~FranklinInterpreterImpl() = default;
};

} // namespace franklin

extern "C" {

using namespace franklin;

// Generic column creation
FranklinColumn* franklin_column_create(FranklinDataType type, size_t size,
                                       double value) {
  try {
    switch (type) {
    case FRANKLIN_INT32: {
      auto* col =
          new column_vector<Int32Policy>(size, static_cast<int32_t>(value));
      return reinterpret_cast<FranklinColumn*>(new FranklinColumnImpl(col));
    }
    case FRANKLIN_FLOAT32: {
      auto* col =
          new column_vector<Float32Policy>(size, static_cast<float>(value));
      return reinterpret_cast<FranklinColumn*>(new FranklinColumnImpl(col));
    }
    default:
      return nullptr;
    }
  } catch (...) {
    return nullptr;
  }
}

// Legacy type-specific creators
FranklinColumn* franklin_column_create_int32(size_t size, int32_t value) {
  return franklin_column_create(FRANKLIN_INT32, size,
                                static_cast<double>(value));
}

FranklinColumn* franklin_column_create_float32(size_t size, float value) {
  return franklin_column_create(FRANKLIN_FLOAT32, size,
                                static_cast<double>(value));
}

void franklin_column_destroy(FranklinColumn* col) {
  if (col) {
    delete reinterpret_cast<FranklinColumnImpl*>(col);
  }
}

// ========== Interpreter API ==========

FranklinInterpreter* franklin_interpreter_create_int32() {
  try {
    return reinterpret_cast<FranklinInterpreter*>(
        new FranklinInterpreterImpl());
  } catch (...) {
    return nullptr;
  }
}

void franklin_interpreter_destroy(FranklinInterpreter* interp) {
  if (interp) {
    delete reinterpret_cast<FranklinInterpreterImpl*>(interp);
  }
}

bool franklin_interpreter_register(FranklinInterpreter* interp,
                                   const char* name, FranklinColumn* col) {
  if (!interp || !name || !col)
    return false;

  try {
    auto* interp_impl = reinterpret_cast<FranklinInterpreterImpl*>(interp);
    auto* col_impl = reinterpret_cast<FranklinColumnImpl*>(col);

    // Register based on policy type
    if (col_impl->policy_id == DataTypeEnum::Int32Default) {
      auto* typed_col = col_impl->get_as<Int32Policy>();
      interp_impl->interp.register_column(std::string(name),
                                          std::move(*typed_col));
      delete col_impl;
      return true;
    } else if (col_impl->policy_id == DataTypeEnum::Float32Default) {
      auto* typed_col = col_impl->get_as<Float32Policy>();
      interp_impl->interp.register_column(std::string(name),
                                          std::move(*typed_col));
      delete col_impl;
      return true;
    }

    return false;
  } catch (...) {
    return false;
  }
}

FranklinColumn* franklin_interpreter_eval(FranklinInterpreter* interp,
                                          const char* expression) {
  if (!interp || !expression)
    return nullptr;

  try {
    auto* interp_impl = reinterpret_cast<FranklinInterpreterImpl*>(interp);
    auto result = interp_impl->interp.eval(std::string(expression));

    // Extract the typed column from the erased column
    DataTypeEnum::Enum policy = result.get_policy();

    if (policy == DataTypeEnum::Int32Default) {
      auto* typed_col = result.get_as<Int32Policy>();
      // Copy (don't move) the data - result owns the pointer and will delete it
      auto* result_col = new column_vector<Int32Policy>(*typed_col);
      return reinterpret_cast<FranklinColumn*>(
          new FranklinColumnImpl(result_col));
    } else if (policy == DataTypeEnum::Float32Default) {
      auto* typed_col = result.get_as<Float32Policy>();
      // Copy (don't move) the data - result owns the pointer and will delete it
      auto* result_col = new column_vector<Float32Policy>(*typed_col);
      return reinterpret_cast<FranklinColumn*>(
          new FranklinColumnImpl(result_col));
    }

    return nullptr;
  } catch (...) {
    return nullptr;
  }
}

bool franklin_interpreter_has_column(const FranklinInterpreter* interp,
                                     const char* name) {
  if (!interp || !name)
    return false;

  try {
    auto* interp_impl =
        reinterpret_cast<const FranklinInterpreterImpl*>(interp);
    return interp_impl->interp.has_column(std::string(name));
  } catch (...) {
    return false;
  }
}

size_t franklin_interpreter_size(const FranklinInterpreter* interp) {
  if (!interp)
    return 0;

  try {
    auto* interp_impl =
        reinterpret_cast<const FranklinInterpreterImpl*>(interp);
    return interp_impl->interp.size();
  } catch (...) {
    return 0;
  }
}

// Column accessors
size_t franklin_column_size(const FranklinColumn* col) {
  if (!col)
    return 0;

  try {
    auto* impl = reinterpret_cast<const FranklinColumnImpl*>(col);
    return impl->get_size();
  } catch (...) {
    return 0;
  }
}

int32_t franklin_column_get_int32(const FranklinColumn* col, size_t index) {
  if (!col)
    return 0;

  try {
    auto* impl = reinterpret_cast<const FranklinColumnImpl*>(col);
    return impl->get_int32(index);
  } catch (...) {
    return 0;
  }
}

float franklin_column_get_float32(const FranklinColumn* col, size_t index) {
  if (!col)
    return 0.0f;

  try {
    auto* impl = reinterpret_cast<const FranklinColumnImpl*>(col);
    return impl->get_float32(index);
  } catch (...) {
    return 0.0f;
  }
}

// Utility
const char* franklin_version() {
  return "Franklin 0.1.0";
}

} // extern "C"
