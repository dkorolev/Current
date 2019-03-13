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

// TODO(dkorolev): Test several `jit_call_context`-s over the same function (once we have differentiation).

#include "jit.h"

#include "../../3rdparty/gtest/gtest-main.h"

#ifdef FNCAS_X64_NATIVE_JIT_ENABLED

TEST(OptimizationJIT, SmokeAdd) {
  using namespace current::expression;

  VarsContext context;

  x["a"] = 1.0;
  value_t const value = x["a"] + x["a"];

  // The call to `Freeze()` fixes the variables and nodes used.
  VarsMapperConfig const vars_config = context.Freeze();

  // The constuctor of `JITCallContext` allocates the RAM buffer for the temporary computations.
  jit::JITCallContext jit_call_context(vars_config);

  // The instance of `JITCompiler` can emit one or more compiled functiont, which would all operate on the same
  // instance of `JITCallContext`, so that they, when called in the order of compilation, reuse intermediate results.
  jit::JITCompiler code_generator(jit_call_context);
  jit::Function const f = code_generator.Compile(value);

  VarsMapper input(vars_config);
  EXPECT_EQ(2.0, f(jit_call_context, input.x));

  input["a"] = 2.0;
  EXPECT_EQ(4.0, f(jit_call_context, input.x));

  // Other calling synopsis.
  input["a"] = -2.0;
  EXPECT_EQ(-4.0, f(jit_call_context, &input.x[0]));

  EXPECT_EQ(5.0, f(jit_call_context, {2.5}));
}

TEST(OptimizationJIT, SmokeAddConstant) {
  using namespace current::expression;

  VarsContext context;

  x["b"] = 1.0;
  value_t const value = x["b"] + 1.0;

  // No need for `context.Freeze()`, it will happen automatically in the default constructor of `JITCallContext`.
  jit::JITCallContext jit_call_context;

  jit::Function const f = jit::JITCompiler(jit_call_context).Compile(value);

  VarsMapper input(jit_call_context.Config());
  EXPECT_EQ(2.0, f(jit_call_context, input.x));

  input["b"] = 2.0;
  EXPECT_EQ(3.0, f(jit_call_context, input));  // Can pass `input` instead of `input.x`.

  input["b"] = -2.0;
  EXPECT_EQ(-1.0, f(jit_call_context, input));
}

TEST(OptimizationJIT, SmokeFunctionReturningVector) {
  using namespace current::expression;

  VarsContext context;

  x["a"] = 1.0;
  x["b"] = 1.0;
  std::vector<value_t> const values({x["a"] + x["b"], x["a"] - x["b"], x["a"] * x["b"], x["a"] / x["b"]});

  jit::JITCallContext jit_call_context;

  jit::FunctionReturningVector const g = jit::JITCompiler(jit_call_context).Compile(values);

  {
    VarsMapper input(jit_call_context.Config());
    input["a"] = 10.0;
    input["b"] = 5.0;
    EXPECT_EQ("[15.0,5.0,50.0,2.0]", JSON(g(jit_call_context, input.x)));
  }

  EXPECT_EQ("[6.0,2.0,8.0,2.0]", JSON(g(jit_call_context, {4.0, 2.0})));
}

TEST(OptimizationJIT, FunctionReturningVectorHasUpperBoundOnDim) {
  using namespace current::expression;

  VarsContext context;

  x["a"] = 0.0;
  value_t const v = x["a"];

  std::vector<value_t> const values_2({v + 1, v + 2});
  std::vector<value_t> const values_3({v + 1, v + 2, v + 3});
  std::vector<value_t> const values_4({v + 1, v + 2, v + 3, v + 4});
  std::vector<value_t> const values_5({v + 1, v + 2, v + 3, v + 4, v + 5});
  std::vector<value_t> const values_6({v + 1, v + 2, v + 3, v + 4, v + 5, v + 6});

  {
    // For one variable the default number of extra allocated nodes is five.
    // Thus, `values_5` can successfully compile as a function returning a vector; `values_6` already can not.
    jit::JITCallContext ctx;
    EXPECT_EQ("[1.0,2.0]", JSON(jit::JITCompiler(ctx).Compile(values_2)(ctx, {0.0})));
    EXPECT_EQ("[1.0,2.0,3.0]", JSON(jit::JITCompiler(ctx).Compile(values_3)(ctx, {0.0})));
    EXPECT_EQ("[1.0,2.0,3.0,4.0]", JSON(jit::JITCompiler(ctx).Compile(values_4)(ctx, {0.0})));
    EXPECT_EQ("[1.0,2.0,3.0,4.0,5.0]", JSON(jit::JITCompiler(ctx).Compile(values_5)(ctx, {0.0})));
    ASSERT_THROW(jit::JITCompiler(ctx).Compile(values_6), jit::JITNotEnoughExtraNodesAllocatedInJITCallContext);
  }

  {
    jit::JITCallContext ctx(6u);  // Explicitly providing `6` as the parameter makes `values_6` runnable.
    EXPECT_EQ("[1.0,2.0,3.0,4.0,5.0]", JSON(jit::JITCompiler(ctx).Compile(values_5)(ctx, {0.0})));
    EXPECT_EQ("[1.0,2.0,3.0,4.0,5.0,6.0]", JSON(jit::JITCompiler(ctx).Compile(values_6)(ctx, {0.0})));
  }

  {
    jit::JITCallContext ctx(2u);  // Explicitly providing `2` makes `values_3` go out of bounds.
    EXPECT_EQ("[1.0,2.0]", JSON(jit::JITCompiler(ctx).Compile(values_2)(ctx, {0.0})));
    ASSERT_THROW(jit::JITCompiler(ctx).Compile(values_3), jit::JITNotEnoughExtraNodesAllocatedInJITCallContext);
  }
}

TEST(OptimizationJIT, Exp) {
  using namespace current::expression;

  VarsContext context;

  x["c"] = 0.0;
  value_t const value = exp(x["c"]);

  // No need to provide `context`, the thread-local singleton will be used by default, and it will be `.Freeze()`-ed.
  jit::JITCallContext jit_call_context;

  // Confirm that the lifetime of `JITCompiler` is not necessary for the functions to be called.
  std::unique_ptr<jit::JITCompiler> disposable_code_generator = std::make_unique<jit::JITCompiler>(jit_call_context);

  jit::Function const f = [&]() {
    // Confirm that the very instance of `JITCompiler` does not have to live for the function(s) to be called,
    // it's the lifetime of `JITCallContext` that is important.
    jit::JITCompiler disposable_code_generator(jit_call_context);
    return disposable_code_generator.Compile(value);
  }();

  VarsMapper input(disposable_code_generator->Config());

  disposable_code_generator = nullptr;

  EXPECT_EQ(exp(0.0), f(jit_call_context, input));

  input["c"] = 1.0;
  EXPECT_EQ(exp(1.0), f(jit_call_context, input));

  input["c"] = 2.0;
  EXPECT_EQ(exp(2.0), f(jit_call_context, input));

  input["c"] = -1.0;
  EXPECT_EQ(exp(-1.0), f(jit_call_context, input));

  input["c"] = -2.0;
  EXPECT_EQ(exp(-2.0), f(jit_call_context, input));
}

TEST(OptimizationJIT, NeedActiveVarsContext) {
  using namespace current::expression;

  std::unique_ptr<jit::JITCallContext> illegal_jit_context = []() {
    VarsContext vars_context;
    return std::make_unique<jit::JITCallContext>(vars_context.Freeze());
  }();
  ASSERT_THROW(jit::JITCompiler illegal_code_generator(*illegal_jit_context), VarsManagementException);
}

TEST(OptimizationJIT, NoIntersectingGlobalJITCallContextsAllowed) {
  using namespace current::expression;

  VarsContext context;

  jit::JITCallContext jit_call_context;
  ASSERT_THROW(jit::JITCallContext illegal_call_context, VarsAlreadyFrozenException);
}

TEST(OptimizationJIT, JITGeneratorUnfreezesVarsContext) {
  using namespace current::expression;

  VarsContext context;

  { jit::JITCallContext call_context_1; }
  { jit::JITCallContext call_context_2; }
  {
    context.Freeze();
    ASSERT_THROW(jit::JITCallContext illegal_call_context, VarsAlreadyFrozenException);
  }
}

#endif  // FNCAS_X64_NATIVE_JIT_ENABLED
