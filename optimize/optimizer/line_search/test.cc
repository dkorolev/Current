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

#include "line_search.h"

#include "../../../bricks/graph/gnuplot.h"

#include "../../../3rdparty/gtest/gtest-main-with-dflags.h"

#ifdef FNCAS_X64_NATIVE_JIT_ENABLED

DEFINE_bool(save_line_search_test_plots, false, "Set to have each 1D optimization regression test to be plotted.");

TEST(OptimizationOptimizerLineSearch, FunctionOfOrderTwo) {
  using namespace current::expression;
  using namespace current::expression::optimizer;

  VarsContext vars_context;

  x[0] = 0.0;

  value_t const f = sqr(x[0] - 3.0);

  OptimizationContext optimization_context(vars_context, f);
  LineSearchContext const line_search_context(optimization_context);

  // The function and its gradient must be computed prior to the line search being invoked, in order for the internal
  // `jit_call_context` nodes to be properly initialized. In the case of multidimensional gradient descent optimization
  // this will happen organically, as no line search begins w/o computing the gradient (and computing the function is
  // the prerequisite for computing the gradient). In the case of explicitly testing the 1D optimizer, the compuatation
  // of both the function and the gradient must be invoked manually beforehand, for each starting point.
  optimization_context.compiled_f(optimization_context.jit_call_context, optimization_context.vars_mapper.x);
  optimization_context.compiled_g(optimization_context.jit_call_context, optimization_context.vars_mapper.x);

  // In case of the function of order two, see `../differentiate/test.cc`, the first and best step is always  `-0.5`.
  EXPECT_NEAR(-0.5, LineSearch(line_search_context).best_step, 1e-6);

  // This step should take the function to its optimum, which, in this case, is the minimum, equals to zero.
  EXPECT_EQ(
      0.0,
      optimization_context.compiled_l(optimization_context.jit_call_context, optimization_context.vars_mapper.x, -0.5));

  EXPECT_EQ("[0.0]", JSON(optimization_context.CurrentPoint()));
  EXPECT_EQ(9.0, optimization_context.ComputeCurrentObjectiveFunctionValue());
  optimization_context.MovePointAlongGradient(-0.5);
  EXPECT_EQ("[3.0]", JSON(optimization_context.CurrentPoint()));
  EXPECT_EQ(0.0, optimization_context.ComputeCurrentObjectiveFunctionValue());
}

inline void SavePlotAndLineSearchPath(std::string const& test_name,
                                      std::string const& function_as_string,
                                      current::expression::optimizer::OptimizationContext const& optimization_context,
                                      current::expression::optimizer::LineSearchResult const& result,
                                      double derivative_value) {
  using namespace current::gnuplot;

#ifndef CURRENT_APPLE
  const char* const format = "pngcairo";
#else
  const char* const format = "png";
#endif
  const char* const extension = "png";

  const std::string plot_body =
      GNUPlot()
          .Title(current::strings::Printf("f(x) = %s\\n%d path1 steps\\n%d path2 steps",
                                          function_as_string.c_str(),
                                          static_cast<int>(result.path1.size()),
                                          static_cast<int>(result.path2.size())))
          .Grid("back")
          .XLabel("x")
          .YLabel("f(x), f'(x), steps")
          .Plot(WithMeta([&result, &optimization_context, derivative_value](Plotter p) {
                  for (current::expression::optimizer::LineSearchResult::IntermediatePoint const& pt : result.path1) {
                    double const y1 = optimization_context.compiled_f(optimization_context.jit_call_context,
                                                                      {
                                                                          pt.step * derivative_value,
                                                                      });
                    double const y2 = optimization_context.compiled_g(optimization_context.jit_call_context,
                                                                      {
                                                                          pt.step * derivative_value,
                                                                      })[0];
                    p(pt.step * derivative_value, 0);
                    p(pt.step * derivative_value, y1);
                    p(pt.step * derivative_value, y2);
                    p(pt.step * derivative_value, 0);
                  }
                  for (current::expression::optimizer::LineSearchResult::IntermediatePoint const& pt : result.path2) {
                    double const y1 = optimization_context.compiled_f(optimization_context.jit_call_context,
                                                                      {
                                                                          pt.step * derivative_value,
                                                                      });
                    double const y2 = optimization_context.compiled_g(optimization_context.jit_call_context,
                                                                      {
                                                                          pt.step * derivative_value,
                                                                      })[0];
                    p(pt.step * derivative_value, 0);
                    p(pt.step * derivative_value, y1);
                    p(pt.step * derivative_value, y2);
                    p(pt.step * derivative_value, 0);
                  }
#if 0
                  for (size_t i = 0u; i < result.path2.size(); ++i) {
                    current::expression::optimizer::LineSearchResult::IntermediatePoint const& pt =
                        result.path2[result.path2.size() - 1u - i];
                    double const x = pt.step * derivative_value;
                    optimization_context.compiled_f(optimization_context.jit_call_context, {x});
                    double const y = optimization_context.compiled_g(optimization_context.jit_call_context, {x})[0];
                    p(x, y);
                  }
#endif
                })
                    .Name("points")
                    .LineWidth(1)
                    .Color("rgb '#D0D0D0'"))
          .Plot(WithMeta([&optimization_context](Plotter p) {
                  for (int i = -50; i <= +1050; ++i) {
                    double const x = 0.01 * i;
                    double const y = optimization_context.compiled_f(optimization_context.jit_call_context, {x});
                    p(x, y);
                  }
                })
                    .Name("f(x)")
                    .LineWidth(10)
                    .Color("rgb '#D0FFD0'"))
          .Plot(WithMeta([&optimization_context](Plotter p) {
                  for (int i = -50; i <= +1050; ++i) {
                    double const x = 0.01 * i;
                    double const unused_but_MUST_be_computed =
                        optimization_context.compiled_f(optimization_context.jit_call_context, {x});
                    static_cast<void>(unused_but_MUST_be_computed);
                    double const y = optimization_context.compiled_g(optimization_context.jit_call_context, {x})[0];
                    p(x, y);
                  }
                })
                    .Name("f'(x)")
                    .LineWidth(5)
                    .Color("rgb '#000000'"))
          .Plot(WithMeta([&result, derivative_value](Plotter p) {
                  for (current::expression::optimizer::LineSearchResult::IntermediatePoint const& pt : result.path1) {
                    p(pt.step * derivative_value, pt.f);
                  }
                })
                    .Name("path1")
                    .LineWidth(2)
                    .Color("rgb '#0000FF'"))
          .Plot(WithMeta([&result, derivative_value](Plotter p) {
                  for (current::expression::optimizer::LineSearchResult::IntermediatePoint const& pt : result.path2) {
                    p(pt.step * derivative_value, pt.f);
                  }
                })
                    .Name("path2")
                    .LineWidth(2)
                    .Color("rgb '#FF0000'"))
          .ImageSize(800)
          .OutputFormat(format);

  // OK to use the non-portable `.current/` here, as this code is a) flag-protected, and b) System V only. -- D.K.
  current::FileSystem::WriteStringToFile(
      plot_body, (".current/" + test_name.substr(0, 2) + '-' + test_name.substr(2) + '.' + extension).c_str());
}

// TODO(dkorolev): Test same functions with different numbers of derivatives taken.
// TODO(dkorolev): Also test the number of steps it took to optimize a function.
#define TEST_1D_LINE_SEARCH(test_name, function_body, expected_final_value, expected_comments)                         \
  TEST(OptimizationOptimizerLineSearch, RegressionTest##test_name) {                                                   \
    using namespace current::expression;                                                                               \
    VarsContext vars_context;                                                                                          \
    x[0] = 0.0;                                                                                                        \
    vars_context.ReindexVars();                                                                                        \
    value_t const f = [](value_t x) { return function_body; }(x[0]);                                                   \
    using namespace current::expression::optimizer;                                                                    \
    OptimizationContext optimization_context(vars_context, f);                                                         \
    LineSearchContext const line_search_context(optimization_context);                                                 \
    optimization_context.compiled_f(optimization_context.jit_call_context, optimization_context.vars_mapper.x);        \
    double const derivative_value =                                                                                    \
        optimization_context.compiled_g(optimization_context.jit_call_context, optimization_context.vars_mapper.x)[0]; \
    LineSearchResult const result = LineSearch(line_search_context);                                                   \
    double const step_size = result.best_step;                                                                         \
    optimization_context.MovePointAlongGradient(step_size);                                                            \
    double const final_value = optimization_context.ComputeCurrentObjectiveFunctionValue();                            \
    if (FLAGS_save_line_search_test_plots) {                                                                           \
      SavePlotAndLineSearchPath(#test_name, #function_body, optimization_context, result, derivative_value);           \
    }                                                                                                                  \
    if (false) {                                                                                                       \
      EXPECT_EQ(expected_comments, current::strings::Join(result.comments, "; "));                                     \
    }                                                                                                                  \
    if (!std::isnan(expected_final_value)) {                                                                           \
      EXPECT_NEAR(expected_final_value, final_value, 1e-6);                                                            \
    }                                                                                                                  \
  }

// This is a simple order-two function, with a clearly visible minumum at `x = 6`, found in a single Newtop step.
TEST_1D_LINE_SEARCH(01Parabola,
                    5 + sqr(x - 6),
                    5.0,
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");

// A modification to the above test to make it an order-three function, so that the first step "overshoots" `x = 6`.
TEST_1D_LINE_SEARCH(02SlightlyCubicParabola,
                    5 + (x - 6) * (x - 6) * (1 + 0.03 * (x - 6)),
                    5.0,
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");

// A sine.
TEST_1D_LINE_SEARCH(03Sine,
                    2 - sin(0.35 * x - 0.75),
                    1.0,
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");

// A piece of a circle.
TEST_1D_LINE_SEARCH(04CircleArc,
                    10 - sqrt(sqr(9) - sqr(x - 6)),
                    1.0,
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");

// An power-(-2) hump.
TEST_1D_LINE_SEARCH(05PowerNegativeTwoHump,
                    2 - 1 / (1 + sqr(x - 6)),
                    1.0,
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");

// A bell-curve-resembling arc.
TEST_1D_LINE_SEARCH(06NormalHump,
                    2 - exp(-sqr(x / 2 - 3)),
                    1.0,
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");

// A valley formed by two softmaxes.
TEST_1D_LINE_SEARCH(07HumpOfTwoSoftmaxes,
                    2 + (log(1 + exp(x - 6)) + log(1 + exp(6 - x))),
                    2.0 + 2.0 * log(2.0),
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");

// The shapes resembling "The Little Prince". Lessons learned:
// 1) Bell curves really get to zero derivative quickly. But regularization helps a lot. `+ 0.001 * sqr(x - 5)` is it.
// 2) Looking for the zero of the derivative carries the risk of missing a better minimum.
//    Not sure if it's important for real-life problems; something to look into later.
TEST_1D_LINE_SEARCH(08LittlePrince,
                    2 - exp(-sqr(x - 6)) - 0.2 * exp(-sqr(x - 4)) + 0.001 * sqr(x - 5),
                    std::numeric_limits<double>::quiet_NaN(),
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");
TEST_1D_LINE_SEARCH(09LittlePrince,
                    2 - exp(-sqr(x - 6)) - 0.3 * exp(-sqr(x - 4)) + 0.001 * sqr(x - 5),
                    std::numeric_limits<double>::quiet_NaN(),
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");
TEST_1D_LINE_SEARCH(10LittlePrince,
                    2 - exp(-sqr(x - 6)) - 0.4 * exp(-sqr(x - 4)) + 0.001 * sqr(x - 5),
                    std::numeric_limits<double>::quiet_NaN(),
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");
TEST_1D_LINE_SEARCH(11LittlePrince,
                    2 - exp(-sqr(x - 6)) - 0.5 * exp(-sqr(x - 4)) + 0.001 * sqr(x - 5),
                    std::numeric_limits<double>::quiet_NaN(),
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");
TEST_1D_LINE_SEARCH(12LittlePrince,
                    2 - exp(-sqr(x - 6)) - 0.6 * exp(-sqr(x - 4)) + 0.001 * sqr(x - 5),
                    std::numeric_limits<double>::quiet_NaN(),
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");
TEST_1D_LINE_SEARCH(13LittlePrince,
                    2 - exp(-sqr(x - 6)) - 0.02 * exp(-sqr(x - 3)) + 0.001 * sqr(x - 5),
                    std::numeric_limits<double>::quiet_NaN(),
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");
TEST_1D_LINE_SEARCH(14LittlePrince,
                    2 - exp(-sqr(x - 6)) - 0.03 * exp(-sqr(x - 3)) + 0.001 * sqr(x - 5),
                    std::numeric_limits<double>::quiet_NaN(),
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");
TEST_1D_LINE_SEARCH(15LittlePrince,
                    2 - exp(-sqr(x - 6)) - 0.04 * exp(-sqr(x - 3)) + 0.001 * sqr(x - 5),
                    std::numeric_limits<double>::quiet_NaN(),
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");
TEST_1D_LINE_SEARCH(16LittlePrince,
                    2 - exp(-sqr(x - 6)) - 0.05 * exp(-sqr(x - 3)) + 0.001 * sqr(x - 5),
                    std::numeric_limits<double>::quiet_NaN(),
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");
TEST_1D_LINE_SEARCH(17LittlePrince,
                    2 - exp(-sqr(x - 6)) - 0.06 * exp(-sqr(x - 3)) + 0.001 * sqr(x - 5),
                    std::numeric_limits<double>::quiet_NaN(),
                    "perfect search range located; TODO(dkorolev): suboptimal, but solution found using binary search");

#undef TEST_1D_LINE_SEARCH

#endif  // FNCAS_X64_NATIVE_JIT_ENABLED