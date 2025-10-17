#ifndef FRANKLIN_PYTHON_C_API_H
#define FRANKLIN_PYTHON_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Opaque handles
typedef struct FranklinColumn FranklinColumn;
typedef struct FranklinInterpreter FranklinInterpreter;

// Data types supported
typedef enum {
  FRANKLIN_INT32 = 0,
  FRANKLIN_FLOAT32 = 1,
} FranklinDataType;

// ========== Column API ==========

// Generic column creation - value is interpreted based on type
// For INT32: pass int32_t cast to double
// For FLOAT32: pass float cast to double
FranklinColumn* franklin_column_create(FranklinDataType type, size_t size,
                                       double value);

// Legacy type-specific creators (for backwards compatibility)
FranklinColumn* franklin_column_create_int32(size_t size, int32_t value);
FranklinColumn* franklin_column_create_float32(size_t size, float value);

void franklin_column_destroy(FranklinColumn* col);

// Column accessors
size_t franklin_column_size(const FranklinColumn* col);
int32_t franklin_column_get_int32(const FranklinColumn* col, size_t index);
float franklin_column_get_float32(const FranklinColumn* col, size_t index);

// ========== Interpreter API ==========

// Create/destroy interpreter
FranklinInterpreter* franklin_interpreter_create_int32();
void franklin_interpreter_destroy(FranklinInterpreter* interp);

// Register a column with a name
// Note: The interpreter takes ownership of the column
bool franklin_interpreter_register(FranklinInterpreter* interp,
                                   const char* name, FranklinColumn* col);

// Evaluate an expression
// Returns a new column (caller must call franklin_column_destroy on it)
// Returns NULL on error
FranklinColumn* franklin_interpreter_eval(FranklinInterpreter* interp,
                                          const char* expression);

// Check if a column is registered
bool franklin_interpreter_has_column(const FranklinInterpreter* interp,
                                     const char* name);

// Get number of registered columns
size_t franklin_interpreter_size(const FranklinInterpreter* interp);

// ========== Utility ==========

const char* franklin_version();

#ifdef __cplusplus
}
#endif

#endif // FRANKLIN_PYTHON_C_API_H
