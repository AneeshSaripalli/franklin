#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace franklin::expr {

// Data type enumeration for runtime type identification
enum class DataType : uint8_t {
  Int8,
  Int16,
  Int32,
  Int64,
  UInt8,
  UInt16,
  UInt32,
  UInt64,
  Float16,
  Float32,
  Float64,
  BF16,
  Bool,
  Unknown
};

// Operation type enumeration
enum class OpType : uint8_t {
  // Arithmetic
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  // Bitwise
  BitAnd,
  BitOr,
  BitXor,
  BitNot,
  Shl,
  Shr,
  // Comparison
  Eq,
  Ne,
  Lt,
  Le,
  Gt,
  Ge,
  // Logical
  LogicalAnd,
  LogicalOr,
  LogicalNot,
  // Special
  FMA, // Fused multiply-add: a * b + c
  Min,
  Max
};

// Forward declaration for visitor
class ExprVisitor;

// Base expression node with metadata for optimization
struct ExprNode {
  DataType result_type = DataType::Unknown;

  // Cost model metadata
  uint32_t complexity = 0;   // Rough op count
  uint32_t memory_loads = 0; // Memory bandwidth estimate
  bool is_constant = false;  // Can be evaluated at compile time

  // Fusion metadata
  bool can_fuse = true;          // Can this node be fused?
  uint8_t register_pressure = 0; // Estimated SIMD register usage

  virtual ~ExprNode() = default;
  virtual void accept(ExprVisitor& v) = 0;
  virtual std::unique_ptr<ExprNode> clone() const = 0;
};

// Leaf node: column reference
struct ColumnRefNode : ExprNode {
  std::string name;
  size_t column_index = 0;

  ColumnRefNode() = default;
  ColumnRefNode(const std::string& n, size_t idx = 0)
      : name(n), column_index(idx) {
    complexity = 0;
    memory_loads = 1;
    is_constant = false;
  }

  void accept(ExprVisitor& v) override;
  std::unique_ptr<ExprNode> clone() const override;
};

// Constant node: scalar value
struct ConstantNode : ExprNode {
  union {
    int64_t i64;
    uint64_t u64;
    double f64;
    bool b;
  } value;

  ConstantNode() {
    value.i64 = 0;
    is_constant = true;
    complexity = 0;
    memory_loads = 0;
  }

  explicit ConstantNode(int64_t v) : ConstantNode() {
    value.i64 = v;
    result_type = DataType::Int64;
  }

  explicit ConstantNode(uint64_t v) : ConstantNode() {
    value.u64 = v;
    result_type = DataType::UInt64;
  }

  explicit ConstantNode(double v) : ConstantNode() {
    value.f64 = v;
    result_type = DataType::Float64;
  }

  explicit ConstantNode(bool v) : ConstantNode() {
    value.b = v;
    result_type = DataType::Bool;
  }

  void accept(ExprVisitor& v) override;
  std::unique_ptr<ExprNode> clone() const override;
};

// Unary operation: OP child
struct UnaryOpNode : ExprNode {
  std::unique_ptr<ExprNode> child;
  OpType op;

  UnaryOpNode() = default;
  UnaryOpNode(OpType o, std::unique_ptr<ExprNode> c)
      : child(std::move(c)), op(o) {
    if (child) {
      complexity = child->complexity + 1;
      memory_loads = child->memory_loads;
    }
  }

  void accept(ExprVisitor& v) override;
  std::unique_ptr<ExprNode> clone() const override;
};

// Binary operation: left OP right
struct BinaryOpNode : ExprNode {
  std::unique_ptr<ExprNode> left;
  std::unique_ptr<ExprNode> right;
  OpType op;

  // Fusion hint: is this the root of a fusible subexpression?
  bool fusion_root = false;
  std::vector<const ExprNode*> fusion_group; // Nodes to fuse together

  BinaryOpNode() = default;
  BinaryOpNode(OpType o, std::unique_ptr<ExprNode> l,
               std::unique_ptr<ExprNode> r)
      : left(std::move(l)), right(std::move(r)), op(o) {
    if (left && right) {
      complexity = left->complexity + right->complexity + 1;
      memory_loads = left->memory_loads + right->memory_loads;
      register_pressure =
          left->register_pressure + right->register_pressure + 1;
    }
  }

  void accept(ExprVisitor& v) override;
  std::unique_ptr<ExprNode> clone() const override;
};

// Ternary: condition ? true_expr : false_expr
struct TernaryNode : ExprNode {
  std::unique_ptr<ExprNode> condition;
  std::unique_ptr<ExprNode> true_branch;
  std::unique_ptr<ExprNode> false_branch;

  TernaryNode() = default;
  TernaryNode(std::unique_ptr<ExprNode> cond, std::unique_ptr<ExprNode> true_br,
              std::unique_ptr<ExprNode> false_br)
      : condition(std::move(cond)), true_branch(std::move(true_br)),
        false_branch(std::move(false_br)) {
    if (condition && true_branch && false_branch) {
      complexity = condition->complexity + true_branch->complexity +
                   false_branch->complexity + 1;
      memory_loads = condition->memory_loads + true_branch->memory_loads +
                     false_branch->memory_loads;
      can_fuse = false; // Ternary is complex, don't fuse
    }
  }

  void accept(ExprVisitor& v) override;
  std::unique_ptr<ExprNode> clone() const override;
};

// Explicit type cast: cast<T>(child)
struct CastNode : ExprNode {
  std::unique_ptr<ExprNode> child;
  DataType target_type = DataType::Unknown;

  CastNode() = default;
  CastNode(DataType target, std::unique_ptr<ExprNode> c)
      : child(std::move(c)), target_type(target) {
    result_type = target;
    if (child) {
      complexity = child->complexity + 1;
      memory_loads = child->memory_loads;
      register_pressure = child->register_pressure;
    }
  }

  void accept(ExprVisitor& v) override;
  std::unique_ptr<ExprNode> clone() const override;
};

// Visitor pattern for AST traversal
class ExprVisitor {
public:
  virtual ~ExprVisitor() = default;
  virtual void visit(ColumnRefNode& n) = 0;
  virtual void visit(ConstantNode& n) = 0;
  virtual void visit(UnaryOpNode& n) = 0;
  virtual void visit(BinaryOpNode& n) = 0;
  virtual void visit(TernaryNode& n) = 0;
  virtual void visit(CastNode& n) = 0;
};

// Pattern classification for tier selection
enum class ExprPattern {
  SingleColumn,     // "a" - passthrough
  BinaryOp,         // "a + b" - single SIMD op
  NestedBinary,     // "a + (b * c)" - 2-3 ops, fusible
  FMA,              // "a * b + c" - single FMA instruction
  ComplexFusible,   // 4-8 ops, fusible, worth codegen
  ComplexUnfusible, // Complex with dependencies, need interpreter
};

// Helper functions for type operations
inline const char* type_to_string(DataType type) {
  switch (type) {
  case DataType::Int8:
    return "int8";
  case DataType::Int16:
    return "int16";
  case DataType::Int32:
    return "int32";
  case DataType::Int64:
    return "int64";
  case DataType::UInt8:
    return "uint8";
  case DataType::UInt16:
    return "uint16";
  case DataType::UInt32:
    return "uint32";
  case DataType::UInt64:
    return "uint64";
  case DataType::Float16:
    return "float16";
  case DataType::Float32:
    return "float32";
  case DataType::Float64:
    return "float64";
  case DataType::BF16:
    return "bf16";
  case DataType::Bool:
    return "bool";
  case DataType::Unknown:
    return "unknown";
  }
  return "unknown";
}

inline const char* op_to_string(OpType op) {
  switch (op) {
  case OpType::Add:
    return "+";
  case OpType::Sub:
    return "-";
  case OpType::Mul:
    return "*";
  case OpType::Div:
    return "/";
  case OpType::Mod:
    return "%";
  case OpType::BitAnd:
    return "&";
  case OpType::BitOr:
    return "|";
  case OpType::BitXor:
    return "^";
  case OpType::BitNot:
    return "~";
  case OpType::Shl:
    return "<<";
  case OpType::Shr:
    return ">>";
  case OpType::Eq:
    return "==";
  case OpType::Ne:
    return "!=";
  case OpType::Lt:
    return "<";
  case OpType::Le:
    return "<=";
  case OpType::Gt:
    return ">";
  case OpType::Ge:
    return ">=";
  case OpType::LogicalAnd:
    return "&&";
  case OpType::LogicalOr:
    return "||";
  case OpType::LogicalNot:
    return "!";
  case OpType::FMA:
    return "fma";
  case OpType::Min:
    return "min";
  case OpType::Max:
    return "max";
  }
  return "unknown";
}

inline bool is_floating_type(DataType type) {
  return type == DataType::Float16 || type == DataType::Float32 ||
         type == DataType::Float64 || type == DataType::BF16;
}

inline bool is_integral_type(DataType type) {
  return type == DataType::Int8 || type == DataType::Int16 ||
         type == DataType::Int32 || type == DataType::Int64 ||
         type == DataType::UInt8 || type == DataType::UInt16 ||
         type == DataType::UInt32 || type == DataType::UInt64;
}

inline bool is_signed_type(DataType type) {
  return type == DataType::Int8 || type == DataType::Int16 ||
         type == DataType::Int32 || type == DataType::Int64 ||
         is_floating_type(type);
}

inline bool is_bitwise_op(OpType op) {
  return op == OpType::BitAnd || op == OpType::BitOr || op == OpType::BitXor ||
         op == OpType::BitNot || op == OpType::Shl || op == OpType::Shr;
}

inline bool is_comparison_op(OpType op) {
  return op == OpType::Eq || op == OpType::Ne || op == OpType::Lt ||
         op == OpType::Le || op == OpType::Gt || op == OpType::Ge;
}

inline bool is_logical_op(OpType op) {
  return op == OpType::LogicalAnd || op == OpType::LogicalOr ||
         op == OpType::LogicalNot;
}

inline size_t sizeof_type(DataType type) {
  switch (type) {
  case DataType::Int8:
  case DataType::UInt8:
  case DataType::Bool:
    return 1;
  case DataType::Int16:
  case DataType::UInt16:
  case DataType::Float16:
  case DataType::BF16:
    return 2;
  case DataType::Int32:
  case DataType::UInt32:
  case DataType::Float32:
    return 4;
  case DataType::Int64:
  case DataType::UInt64:
  case DataType::Float64:
    return 8;
  case DataType::Unknown:
    return 0;
  }
  return 0;
}

} // namespace franklin::expr
