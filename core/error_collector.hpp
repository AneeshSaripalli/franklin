#pragma once

#include <cstdint>
#include <mutex>
#include <source_location>
#include <string_view>
#include <vector>

namespace franklin {
namespace core {

enum class ErrorCode : std::uint32_t {
  None = 0,
  OutOfRange,
  InvalidArgument,
  InvalidOperation,
  AllocationFailure,
  // Add more error codes as needed
};

struct ErrorInfo {
  ErrorCode code;
  std::string_view component;    // e.g., "dynamic_bitset"
  std::string_view operation;    // e.g., "test", "set", "flip"
  std::string_view message;      // Human-readable description
  std::source_location location; // Source location where error occurred

  // Context-specific data (up to 4 values)
  std::uint64_t context_data[4];
  std::uint8_t num_context_values;

  ErrorInfo() : code(ErrorCode::None), context_data{}, num_context_values(0) {}

  ErrorInfo(ErrorCode error_code, std::string_view comp, std::string_view op,
            std::string_view msg,
            std::source_location loc = std::source_location::current())
      : code(error_code), component(comp), operation(op), message(msg),
        location(loc), context_data{}, num_context_values(0) {}

  // Add context values (position, size, etc.)
  void add_context(std::uint64_t value) {
    if (num_context_values < 4) {
      context_data[num_context_values++] = value;
    }
  }
};

class ErrorCollector {
private:
  std::vector<ErrorInfo> errors_;
  mutable std::mutex mutex_;
  bool enabled_ = true;

  ErrorCollector() = default;

public:
  // Singleton access
  static ErrorCollector& instance() {
    static ErrorCollector collector;
    return collector;
  }

  // Delete copy/move
  ErrorCollector(const ErrorCollector&) = delete;
  ErrorCollector& operator=(const ErrorCollector&) = delete;
  ErrorCollector(ErrorCollector&&) = delete;
  ErrorCollector& operator=(ErrorCollector&&) = delete;

  // Report an error
  void report(ErrorInfo&& error) {
    if (!enabled_)
      return;

    std::lock_guard<std::mutex> lock(mutex_);
    errors_.push_back(std::move(error));
  }

  // Report with inline construction
  void report(ErrorCode code, std::string_view component,
              std::string_view operation, std::string_view message,
              std::source_location location = std::source_location::current()) {
    report(ErrorInfo(code, component, operation, message, location));
  }

  // Check if there are any errors
  bool has_errors() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !errors_.empty();
  }

  // Get error count
  std::size_t error_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return errors_.size();
  }

  // Get the last error (if any)
  ErrorInfo get_last_error() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (errors_.empty()) {
      return ErrorInfo();
    }
    return errors_.back();
  }

  // Get all errors (copy)
  std::vector<ErrorInfo> get_all_errors() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return errors_;
  }

  // Clear all errors
  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    errors_.clear();
  }

  // Enable/disable error collection
  void set_enabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = enabled;
  }

  bool is_enabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return enabled_;
  }

  // Reserve space for errors (optimization)
  void reserve(std::size_t capacity) {
    std::lock_guard<std::mutex> lock(mutex_);
    errors_.reserve(capacity);
  }
};

// Convenience functions for reporting errors
inline void report_error(ErrorInfo&& error) {
  ErrorCollector::instance().report(std::move(error));
}

inline void
report_error(ErrorCode code, std::string_view component,
             std::string_view operation, std::string_view message,
             std::source_location location = std::source_location::current()) {
  ErrorCollector::instance().report(code, component, operation, message,
                                    location);
}

// Helper to create error with context
inline ErrorInfo
make_error(ErrorCode code, std::string_view component,
           std::string_view operation, std::string_view message,
           std::source_location location = std::source_location::current()) {
  return ErrorInfo(code, component, operation, message, location);
}

} // namespace core
} // namespace franklin
