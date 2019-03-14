/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2019 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#ifndef OPTIMIZE_DIFFERENTIATE_DIFFERENTIATE_H
#define OPTIMIZE_DIFFERENTIATE_DIFFERENTIATE_H

#include "../base.h"
#include "../expression/expression.h"
#include "../vars/vars.h"

#include <cmath>

namespace current {
namespace expression {

struct DifferentiatorNotImplementedException final : OptimizeException {};

struct DifferentiationDeliberatelyNotImplemented : OptimizeException {};
struct DoNotDifferentiateUnitStepException final : DifferentiationDeliberatelyNotImplemented {};
struct DoNotDifferentiateSigmoidException final : DifferentiationDeliberatelyNotImplemented {};

class Differentiator {
 private:
  VarsContext const& vars_context_;
  size_t const derivative_per_finalized_var_index_;

 public:
  Differentiator(VarsContext const& vars_context, size_t derivative_per_finalized_var_index)
      : vars_context_(vars_context), derivative_per_finalized_var_index_(derivative_per_finalized_var_index) {}

  value_t Differentiate(value_t f) {
    expression_node_index_t const index = static_cast<expression_node_index_t>(ExpressionNodeIndex(f));
    if (~index < index) {
      // Dealing with a variable, possibly a constant one. So, the derivative is zero or one.
      // TODO(dkorolev): Do not, of course, allocate independent expression nodes for every single zero and one.
      return ExpressionNode::FromImmediateDouble(
          vars_context_.LeafDerivativeZeroOrOne(~index, derivative_per_finalized_var_index_));
    } else {
      ExpressionNodeImpl const& node = vars_context_[index];
      if (node.type_ == ExpressionNodeType::ImmediateDouble) {
        // NOTE(dkorolev): This should not happen as I move to the compact form of `ExpressionNodeImpl`.
        // TODO(dkorolev): This piece of code will go away once I do refactor.
        // CURRENT_THROW(DifferentiatorNotImplementedException());
        return ExpressionNode::FromImmediateDouble(0.0);
      } else if (node.type_ == ExpressionNodeType::Operation_add) {
        value_t const a = ExpressionNode::FromIndex(node.lhs_);
        value_t const b = ExpressionNode::FromIndex(node.rhs_);
        return Differentiate(a) + Differentiate(b);
      } else if (node.type_ == ExpressionNodeType::Operation_sub) {
        value_t const a = ExpressionNode::FromIndex(node.lhs_);
        value_t const b = ExpressionNode::FromIndex(node.rhs_);
        return Differentiate(a) - Differentiate(b);
      } else if (node.type_ == ExpressionNodeType::Operation_mul) {
        value_t const a = ExpressionNode::FromIndex(node.lhs_);
        value_t const b = ExpressionNode::FromIndex(node.rhs_);
        return a * Differentiate(b) + b * Differentiate(a);
      } else if (node.type_ == ExpressionNodeType::Operation_div) {
        value_t const a = ExpressionNode::FromIndex(node.lhs_);
        value_t const b = ExpressionNode::FromIndex(node.rhs_);
        value_t const da = Differentiate(a);
        value_t const db = Differentiate(b);
        return (b * da - a * db) / (b * b);
      } else if (node.type_ == ExpressionNodeType::Function_exp) {
        value_t const x = ExpressionNode::FromIndex(node.lhs_);
        value_t const dx = Differentiate(x);
        return dx * f;
      } else if (node.type_ == ExpressionNodeType::Function_log) {
        value_t const x = ExpressionNode::FromIndex(node.lhs_);
        value_t const dx = Differentiate(x);
        return dx / x;
      } else if (node.type_ == ExpressionNodeType::Function_sin) {
        value_t const x = ExpressionNode::FromIndex(node.lhs_);
        value_t const dx = Differentiate(x);
        return dx * cos(x);
      } else if (node.type_ == ExpressionNodeType::Function_cos) {
        value_t const x = ExpressionNode::FromIndex(node.lhs_);
        value_t const dx = Differentiate(x);
        return -dx * sin(x);
      } else if (node.type_ == ExpressionNodeType::Function_tan) {
        value_t const x = ExpressionNode::FromIndex(node.lhs_);
        value_t const dx = Differentiate(x);
        return dx / sqr(cos(x));
      } else if (node.type_ == ExpressionNodeType::Function_sqr) {
        value_t const x = ExpressionNode::FromIndex(node.lhs_);
        value_t const dx = Differentiate(x);
        return dx * 2.0 * x;
      } else if (node.type_ == ExpressionNodeType::Function_sqrt) {
        value_t const x = ExpressionNode::FromIndex(node.lhs_);
        value_t const dx = Differentiate(x);
        return dx / (2.0 * f);
      } else if (node.type_ == ExpressionNodeType::Function_asin) {
        value_t const x = ExpressionNode::FromIndex(node.lhs_);
        value_t const dx = Differentiate(x);
        return dx / sqrt(1.0 - sqr(x));
      } else if (node.type_ == ExpressionNodeType::Function_acos) {
        value_t const x = ExpressionNode::FromIndex(node.lhs_);
        value_t const dx = Differentiate(x);
        return -dx / sqrt(1.0 - sqr(x));
      } else if (node.type_ == ExpressionNodeType::Function_atan) {
        value_t const x = ExpressionNode::FromIndex(node.lhs_);
        value_t const dx = Differentiate(x);
        return dx / (1.0 + sqr(x));
      } else if (node.type_ == ExpressionNodeType::Function_unit_step) {
        CURRENT_THROW(DoNotDifferentiateUnitStepException());
      } else if (node.type_ == ExpressionNodeType::Function_ramp) {
        value_t const x = ExpressionNode::FromIndex(node.lhs_);
        value_t const dx = Differentiate(x);
        return dx * unit_step(x);
      } else if (node.type_ == ExpressionNodeType::Function_sigmoid) {
        CURRENT_THROW(DoNotDifferentiateSigmoidException());
      } else if (node.type_ == ExpressionNodeType::Function_log_sigmoid) {
        value_t const x = ExpressionNode::FromIndex(node.lhs_);
        value_t const dx = Differentiate(x);
        return dx * sigmoid(-x);
      } else {
        CURRENT_THROW(DifferentiatorNotImplementedException());
      }
    }
  }
};

inline value_t Differentiate(value_t f, size_t derivative_per_finalized_var_index) {
  return Differentiator(VarsManager::TLS().Active(), derivative_per_finalized_var_index).Differentiate(f);
}

}  // namespace current::expression
}  // namespace current

#endif  // #ifndef OPTIMIZE_DIFFERENTIATE_DIFFERENTIATE_H