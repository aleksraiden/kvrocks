/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

#pragma once

#include <fmt/format.h>

#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "fmt/core.h"
#include "ir_iterator.h"
#include "string_util.h"
#include "type_util.h"

// kqir stands for Kvorcks Query Intermediate Representation
namespace kqir {

struct Node {
  virtual std::string Dump() const = 0;
  virtual std::string_view Name() const = 0;
  virtual std::string Content() const { return {}; }

  virtual NodeIterator ChildBegin() { return {}; };
  virtual NodeIterator ChildEnd() { return {}; };

  virtual ~Node() = default;

  template <typename T, typename U = Node, typename... Args>
  static std::unique_ptr<U> Create(Args &&...args) {
    return std::unique_ptr<U>(new T(std::forward<Args>(args)...));
  }

  template <typename T, typename U>
  static std::unique_ptr<T> MustAs(std::unique_ptr<U> &&original) {
    auto casted = As<T>(std::move(original));
    CHECK(casted != nullptr);
    return casted;
  }

  template <typename T, typename U>
  static std::unique_ptr<T> As(std::unique_ptr<U> &&original) {
    auto casted = dynamic_cast<T *>(original.get());
    if (casted) original.release();
    return std::unique_ptr<T>(casted);
  }
};

struct FieldRef : Node {
  std::string name;

  explicit FieldRef(std::string name) : name(std::move(name)) {}

  std::string_view Name() const override { return "FieldRef"; }
  std::string Dump() const override { return name; }
  std::string Content() const override { return Dump(); }
};

struct StringLiteral : Node {
  std::string val;

  explicit StringLiteral(std::string val) : val(std::move(val)) {}

  std::string_view Name() const override { return "StringLiteral"; }
  std::string Dump() const override { return fmt::format("\"{}\"", util::EscapeString(val)); }
  std::string Content() const override { return Dump(); }
};

struct QueryExpr : Node {};

struct BoolAtomExpr : QueryExpr {};

struct TagContainExpr : BoolAtomExpr {
  std::unique_ptr<FieldRef> field;
  std::unique_ptr<StringLiteral> tag;

  TagContainExpr(std::unique_ptr<FieldRef> &&field, std::unique_ptr<StringLiteral> &&tag)
      : field(std::move(field)), tag(std::move(tag)) {}

  std::string_view Name() const override { return "TagContainExpr"; }
  std::string Dump() const override { return fmt::format("{} hastag {}", field->Dump(), tag->Dump()); }

  NodeIterator ChildBegin() override { return {field.get(), tag.get()}; };
  NodeIterator ChildEnd() override { return {}; };
};

struct NumericLiteral : Node {
  double val;

  explicit NumericLiteral(double val) : val(val) {}

  std::string_view Name() const override { return "NumericLiteral"; }
  std::string Dump() const override { return fmt::format("{}", val); }
  std::string Content() const override { return Dump(); }
};

// NOLINTNEXTLINE
#define KQIR_NUMERIC_COMPARE_OPS(X) \
  X(EQ, =, NE, EQ) X(NE, !=, EQ, NE) X(LT, <, GET, GT) X(LET, <=, GT, GET) X(GT, >, LET, LT) X(GET, >=, LT, LET)

struct NumericCompareExpr : BoolAtomExpr {
  enum Op {
#define X(n, s, o, f) n,  // NOLINT
    KQIR_NUMERIC_COMPARE_OPS(X)
#undef X
  } op;
  std::unique_ptr<FieldRef> field;
  std::unique_ptr<NumericLiteral> num;

  NumericCompareExpr(Op op, std::unique_ptr<FieldRef> &&field, std::unique_ptr<NumericLiteral> &&num)
      : op(op), field(std::move(field)), num(std::move(num)) {}

  static constexpr const char *ToOperator(Op op) {
    switch (op) {
// NOLINTNEXTLINE
#define X(n, s, o, f) \
  case n:             \
    return #s;
      KQIR_NUMERIC_COMPARE_OPS(X)
#undef X
    }

    return nullptr;
  }

  static constexpr std::optional<Op> FromOperator(std::string_view op) {
// NOLINTNEXTLINE
#define X(n, s, o, f) \
  if (op == #s) return n;
    KQIR_NUMERIC_COMPARE_OPS(X)
#undef X

    return std::nullopt;
  }

  static constexpr Op Negative(Op op) {
    switch (op) {
// NOLINTNEXTLINE
#define X(n, s, o, f) \
  case n:             \
    return o;
      KQIR_NUMERIC_COMPARE_OPS(X)
#undef X
    }

    __builtin_unreachable();
  }

  static constexpr Op Flip(Op op) {
    switch (op) {
// NOLINTNEXTLINE
#define X(n, s, o, f) \
  case n:             \
    return f;
      KQIR_NUMERIC_COMPARE_OPS(X)
#undef X
    }

    __builtin_unreachable();
  }

  std::string_view Name() const override { return "NumericCompareExpr"; }
  std::string Dump() const override { return fmt::format("{} {} {}", field->Dump(), ToOperator(op), num->Dump()); };
  std::string Content() const override { return ToOperator(op); }

  NodeIterator ChildBegin() override { return {field.get(), num.get()}; };
  NodeIterator ChildEnd() override { return {}; };
};

struct BoolLiteral : BoolAtomExpr {
  bool val;

  explicit BoolLiteral(bool val) : val(val) {}

  std::string_view Name() const override { return "BoolLiteral"; }
  std::string Dump() const override { return val ? "true" : "false"; }
  std::string Content() const override { return Dump(); }
};

struct QueryExpr;

struct NotExpr : QueryExpr {
  std::unique_ptr<QueryExpr> inner;

  explicit NotExpr(std::unique_ptr<QueryExpr> &&inner) : inner(std::move(inner)) {}

  std::string_view Name() const override { return "NotExpr"; }
  std::string Dump() const override { return fmt::format("not {}", inner->Dump()); }

  NodeIterator ChildBegin() override { return NodeIterator{inner.get()}; };
  NodeIterator ChildEnd() override { return {}; };
};

struct AndExpr : QueryExpr {
  std::vector<std::unique_ptr<QueryExpr>> inners;

  explicit AndExpr(std::vector<std::unique_ptr<QueryExpr>> &&inners) : inners(std::move(inners)) {}

  std::string_view Name() const override { return "AndExpr"; }
  std::string Dump() const override {
    return fmt::format("(and {})", util::StringJoin(inners, [](const auto &v) { return v->Dump(); }));
  }

  NodeIterator ChildBegin() override { return NodeIterator(inners.begin()); };
  NodeIterator ChildEnd() override { return NodeIterator(inners.end()); };
};

struct OrExpr : QueryExpr {
  std::vector<std::unique_ptr<QueryExpr>> inners;

  explicit OrExpr(std::vector<std::unique_ptr<QueryExpr>> &&inners) : inners(std::move(inners)) {}

  std::string_view Name() const override { return "OrExpr"; }
  std::string Dump() const override {
    return fmt::format("(or {})", util::StringJoin(inners, [](const auto &v) { return v->Dump(); }));
  }

  NodeIterator ChildBegin() override { return NodeIterator(inners.begin()); };
  NodeIterator ChildEnd() override { return NodeIterator(inners.end()); };
};

struct Limit : Node {
  size_t offset = 0;
  size_t count = std::numeric_limits<size_t>::max();

  Limit(size_t offset, size_t count) : offset(offset), count(count) {}

  std::string_view Name() const override { return "Limit"; }
  std::string Dump() const override { return fmt::format("limit {}, {}", offset, count); }
  std::string Content() const override { return fmt::format("{}, {}", offset, count); }
};

struct SortBy : Node {
  enum Order { ASC, DESC } order = ASC;
  std::unique_ptr<FieldRef> field;

  SortBy(Order order, std::unique_ptr<FieldRef> &&field) : order(order), field(std::move(field)) {}

  static constexpr const char *OrderToString(Order order) { return order == ASC ? "asc" : "desc"; }

  std::string_view Name() const override { return "SortBy"; }
  std::string Dump() const override { return fmt::format("sortby {}, {}", field->Dump(), OrderToString(order)); }
  std::string Content() const override { return OrderToString(order); }

  NodeIterator ChildBegin() override { return NodeIterator(field.get()); };
  NodeIterator ChildEnd() override { return {}; };
};

struct SelectExpr : Node {
  std::vector<std::unique_ptr<FieldRef>> fields;

  explicit SelectExpr(std::vector<std::unique_ptr<FieldRef>> &&fields) : fields(std::move(fields)) {}

  std::string_view Name() const override { return "SelectExpr"; }
  std::string Dump() const override {
    if (fields.empty()) return "select *";
    return fmt::format("select {}", util::StringJoin(fields, [](const auto &v) { return v->Dump(); }));
  }

  NodeIterator ChildBegin() override { return NodeIterator(fields.begin()); };
  NodeIterator ChildEnd() override { return NodeIterator(fields.end()); };
};

struct IndexRef : Node {
  std::string name;

  explicit IndexRef(std::string name) : name(std::move(name)) {}

  std::string_view Name() const override { return "IndexRef"; }
  std::string Dump() const override { return name; }
  std::string Content() const override { return Dump(); }
};

struct SearchStmt : Node {
  std::unique_ptr<SelectExpr> select_expr;
  std::unique_ptr<IndexRef> index;
  std::unique_ptr<QueryExpr> query_expr;  // optional
  std::unique_ptr<Limit> limit;           // optional
  std::unique_ptr<SortBy> sort_by;        // optional

  SearchStmt(std::unique_ptr<IndexRef> &&index, std::unique_ptr<QueryExpr> &&query_expr, std::unique_ptr<Limit> &&limit,
             std::unique_ptr<SortBy> &&sort_by, std::unique_ptr<SelectExpr> &&select_expr)
      : select_expr(std::move(select_expr)),
        index(std::move(index)),
        query_expr(std::move(query_expr)),
        limit(std::move(limit)),
        sort_by(std::move(sort_by)) {}

  std::string_view Name() const override { return "SearchStmt"; }
  std::string Dump() const override {
    std::string opt;
    if (query_expr) opt += " where " + query_expr->Dump();
    if (sort_by) opt += " " + sort_by->Dump();
    if (limit) opt += " " + limit->Dump();
    return fmt::format("{} from {}{}", select_expr->Dump(), index->Dump(), opt);
  }

  static inline const std::vector<std::function<Node *(Node *)>> ChildMap = {
      NodeIterator::MemFn<&SearchStmt::select_expr>, NodeIterator::MemFn<&SearchStmt::index>,
      NodeIterator::MemFn<&SearchStmt::query_expr>,  NodeIterator::MemFn<&SearchStmt::limit>,
      NodeIterator::MemFn<&SearchStmt::sort_by>,
  };

  NodeIterator ChildBegin() override { return NodeIterator(this, ChildMap.begin()); };
  NodeIterator ChildEnd() override { return NodeIterator(this, ChildMap.end()); };
};

}  // namespace kqir
