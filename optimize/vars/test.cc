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

// TODO(dkorolev): Access to `x[]` from some `object.x[]`, w/o the thread-local singleton.
// TODO(dkorolev): Once the expression builder is here, test uninitialized variables.
// TODO(dkorolev): Possibly refactor freeze/unfreeze to be scope-based, when introducing JIT.

#include "vars.h"

#include "../../3rdparty/gtest/gtest-main.h"
#include "../../3rdparty/gtest/singlequoted.h"

#define EXPECT_VAR_NAME_IS_RIGHT(var) EXPECT_EQ(#var, (var).FullVarName())
#define EXPECT_VAR_NAME_WITH_INDEX_IS_RIGHT(var, index) EXPECT_EQ(#var "{" #index "}", (var).FullVarName())

TEST(OptimizationVars, SparseByInt) {
  using namespace current::expression;
  VarsContext context;
  x[1] = 2;
  x[100] = 101;
  x[42] = 0;
  EXPECT_EQ(0u, x[1].InternalVarIndex());
  EXPECT_EQ(1u, x[100].InternalVarIndex());
  EXPECT_EQ(2u, x[42].InternalVarIndex());
  EXPECT_VAR_NAME_IS_RIGHT(x[1]);
  EXPECT_VAR_NAME_IS_RIGHT(x[100]);
  EXPECT_VAR_NAME_IS_RIGHT(x[42]);
  // The elements in the JSON are ordered, and the `q` index is in the order of introduction of the leaves in this tree.
  // Here and in all the tests below.
  EXPECT_EQ("{'I':{'z':[[1,{'X':{'q':0,'x':2.0}}],[42,{'X':{'q':2,'x':0.0}}],[100,{'X':{'q':1,'x':101.0}}]]}}",
            SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
  ASSERT_THROW(x.DenseDoubleVector(100), VarNodeTypeMismatchException);
  ASSERT_THROW(x["foo"], VarNodeTypeMismatchException);
  ASSERT_THROW(x[1][2], VarNodeTypeMismatchException);
  ASSERT_THROW(x[1]["blah"], VarNodeTypeMismatchException);
  ASSERT_THROW(x[1].DenseDoubleVector(100), VarNodeTypeMismatchException);
  // After the call to `Freeze()`, the `i` index is stamped, it is lexicographical, following the order in the JSON.
  // Here and in all the tests below.
  ASSERT_THROW(context.Unfreeze(), VarsNotFrozenException);
  context.Freeze();
  ASSERT_THROW(context.Freeze(), VarsAlreadyFrozenException);
  EXPECT_EQ(
      "{'I':{'z':["
      "[1,{'X':{'q':0,'i':0,'x':2.0}}],"
      "[42,{'X':{'q':2,'i':1,'x':0.0}}],"
      "[100,{'X':{'q':1,'i':2,'x':101.0}}]"
      "]}}",
      SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
}

TEST(OptimizationVars, SparseByString) {
  using namespace current::expression;
  VarsContext context;
  x["foo"] = 1;
  x["bar"] = 2;
  x["baz"] = 3;
  EXPECT_VAR_NAME_IS_RIGHT(x["foo"]);
  EXPECT_VAR_NAME_IS_RIGHT(x["bar"]);
  EXPECT_VAR_NAME_IS_RIGHT(x["baz"]);
  EXPECT_EQ("{'S':{'z':{'bar':{'X':{'q':1,'x':2.0}},'baz':{'X':{'q':2,'x':3.0}},'foo':{'X':{'q':0,'x':1.0}}}}}",
            SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
  ASSERT_THROW(x.DenseDoubleVector(100), VarNodeTypeMismatchException);
  ASSERT_THROW(x[42], VarNodeTypeMismatchException);
  ASSERT_THROW(x["foo"][2], VarNodeTypeMismatchException);
  ASSERT_THROW(x["foo"]["blah"], VarNodeTypeMismatchException);
  ASSERT_THROW(x["foo"].DenseDoubleVector(100), VarNodeTypeMismatchException);
  context.Freeze();
  EXPECT_EQ(
      "{'S':{'z':{"
      "'bar':{'X':{'q':1,'i':0,'x':2.0}},"
      "'baz':{'X':{'q':2,'i':1,'x':3.0}},"
      "'foo':{'X':{'q':0,'i':2,'x':1.0}}"
      "}}}",
      SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
}

TEST(OptimizationVars, EmptyStringAllowedAsVarName) {
  using namespace current::expression;
  VarsContext context;
  x["ok"] = 1;
  x[""] = 2;
  x["nested"]["also ok"] = 3;
  x["nested"][""] = 4;
  EXPECT_VAR_NAME_IS_RIGHT(x["ok"]);
  EXPECT_VAR_NAME_IS_RIGHT(x[""]);
  EXPECT_VAR_NAME_IS_RIGHT(x["nested"]["ok"]);
  EXPECT_VAR_NAME_IS_RIGHT(x["nested"][""]);
}

TEST(OptimizationVars, DenseVector) {
  using namespace current::expression;
  VarsContext context;
  x.DenseDoubleVector(5);
  x[2] = 2;
  x[4] = 4;
  EXPECT_VAR_NAME_IS_RIGHT(x[2]);
  EXPECT_VAR_NAME_IS_RIGHT(x[4]);
  EXPECT_EQ("{'V':{'z':[{'U':{}},{'U':{}},{'X':{'q':0,'x':2.0}},{'U':{}},{'X':{'q':1,'x':4.0}}]}}",
            SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
  ASSERT_THROW(x[42], VarsManagementException);
  ASSERT_THROW(x["foo"], VarNodeTypeMismatchException);
  x.DenseDoubleVector(5);  // Same size, a valid no-op.
  ASSERT_THROW(x.DenseDoubleVector(100), VarNodeTypeMismatchException);
  x[2] = 2;  // Same value, a valid no-op.
  ASSERT_THROW(x[2] = 3, VarNodeReassignmentAttemptException);
  context.Freeze();
  EXPECT_EQ("{'V':{'z':[{'U':{}},{'U':{}},{'X':{'q':0,'i':0,'x':2.0}},{'U':{}},{'X':{'q':1,'i':1,'x':4.0}}]}}",
            SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
}

TEST(OptimizationVars, InternalVarIndexes) {
  using namespace current::expression;
  VarsContext context;
  x["foo"][1] = 2;
  EXPECT_VAR_NAME_IS_RIGHT(x["foo"][1]);
  // Should keep track of allocated internal leaf indexes.
  EXPECT_EQ(0u, x["foo"][1].InternalVarIndex());
  // These are valid "var paths", but with no leaves allocated (they can still be nodes).
  ASSERT_THROW(x["foo"].InternalVarIndex(), VarIsNotLeafException);
  ASSERT_THROW(x["foo"][0].InternalVarIndex(), VarIsNotLeafException);
  // And for the invalid paths the other exception type is thrown.
  ASSERT_THROW(x["foo"]["bar"].InternalVarIndex(), VarNodeTypeMismatchException);
  ASSERT_THROW(x[0].InternalVarIndex(), VarNodeTypeMismatchException);
}

TEST(OptimizationVars, VarsTreeFinalizedExceptions) {
  using namespace current::expression;
  VarsContext context;
  x["dense"].DenseDoubleVector(2);
  x["sparse"][42] = 42;
  x["strings"]["foo"] = 1;
  EXPECT_VAR_NAME_IS_RIGHT(x["dense"][0]);
  EXPECT_VAR_NAME_IS_RIGHT(x["dense"][1]);
  EXPECT_VAR_NAME_IS_RIGHT(x["sparse"][42]);
  EXPECT_VAR_NAME_IS_RIGHT(x["strings"]["foo"]);
  x.Freeze();
  x["dense"][0];
  x["dense"][1];
  x["sparse"][42];
  x["strings"]["foo"];
  ASSERT_THROW(x["dense"][2], VarsFrozenException);
  ASSERT_THROW(x["sparse"][100], VarsFrozenException);
  ASSERT_THROW(x["strings"]["bar"], VarsFrozenException);
  ASSERT_THROW(x["foo"], VarsFrozenException);
}

TEST(OptimizationVars, UnfreezeAndReindex) {
  using namespace current::expression;
  VarsContext context;
  x.DenseDoubleVector(5);
  x[2] = 2;
  x[4] = 4;
  EXPECT_VAR_NAME_IS_RIGHT(x[2]);
  EXPECT_VAR_NAME_IS_RIGHT(x[4]);
  EXPECT_EQ(2u, context.NumberOfVars());
  EXPECT_EQ("{'V':{'z':[{'U':{}},{'U':{}},{'X':{'q':0,'x':2.0}},{'U':{}},{'X':{'q':1,'x':4.0}}]}}",
            SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
  context.Freeze();
  EXPECT_VAR_NAME_WITH_INDEX_IS_RIGHT(x[2], 0);
  EXPECT_VAR_NAME_WITH_INDEX_IS_RIGHT(x[4], 1);
  EXPECT_EQ(2u, context.NumberOfVars());
  ASSERT_THROW(x[3] = 3, VarsFrozenException);
  EXPECT_EQ(2u, context.NumberOfVars());
  EXPECT_EQ("{'V':{'z':[{'U':{}},{'U':{}},{'X':{'q':0,'i':0,'x':2.0}},{'U':{}},{'X':{'q':1,'i':1,'x':4.0}}]}}",
            SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
  context.Unfreeze();
  // Can add a var, but it won't get an internal, "frozen", index.
  EXPECT_EQ(2u, context.NumberOfVars());
  x[3] = 3;
  EXPECT_EQ(3u, context.NumberOfVars());
  EXPECT_EQ(
      "{'V':{'z':["
      "{'U':{}},"
      "{'U':{}},"
      "{'X':{'q':0,'i':0,'x':2.0}},"
      "{'X':{'q':2,'x':3.0}},"  // No `i`, as `x[3]` is not indexed yet.
      "{'X':{'q':1,'i':1,'x':4.0}}"
      "]}}",
      SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
  // After freezing, an internal index is there.
  EXPECT_VAR_NAME_WITH_INDEX_IS_RIGHT(x[2], 0);
  EXPECT_VAR_NAME_WITH_INDEX_IS_RIGHT(x[4], 1);
  EXPECT_VAR_NAME_IS_RIGHT(x[3]);
  context.Freeze();
  EXPECT_EQ(3u, context.NumberOfVars());
  EXPECT_VAR_NAME_WITH_INDEX_IS_RIGHT(x[2], 0);
  EXPECT_VAR_NAME_WITH_INDEX_IS_RIGHT(x[3], 1);
  EXPECT_VAR_NAME_WITH_INDEX_IS_RIGHT(x[4], 2);
  EXPECT_EQ(
      "{'V':{'z':["
      "{'U':{}},"
      "{'U':{}},"
      "{'X':{'q':0,'i':0,'x':2.0}},"
      "{'X':{'q':2,'i':1,'x':3.0}},"  // The `x[3]` variable is not indexes, with the index of `x[4]` shifting.
      "{'X':{'q':1,'i':2,'x':4.0}}"
      "]}}",
      SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
}

TEST(OptimizationVars, MultiDimensionalIntInt) {
  using namespace current::expression;
  VarsContext context;
  x[1][2] = 3;
  x[4][5] = 6;
  EXPECT_EQ("{'I':{'z':[[1,{'I':{'z':[[2,{'X':{'q':0,'x':3.0}}]]}}],[4,{'I':{'z':[[5,{'X':{'q':1,'x':6.0}}]]}}]]}}",
            SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
  context.Freeze();
  EXPECT_EQ(
      "{'I':{'z':["
      "[1,{'I':{'z':[[2,{'X':{'q':0,'i':0,'x':3.0}}]]}}],"
      "[4,{'I':{'z':[[5,{'X':{'q':1,'i':1,'x':6.0}}]]}}]"
      "]}}",
      SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
}

TEST(OptimizationVars, MultiDimensionalIntString) {
  using namespace current::expression;
  VarsContext context;
  x[1]["foo"] = 2;
  x[3]["bar"] = 4;
  EXPECT_EQ("{'I':{'z':[[1,{'S':{'z':{'foo':{'X':{'q':0,'x':2.0}}}}}],[3,{'S':{'z':{'bar':{'X':{'q':1,'x':4.0}}}}}]]}}",
            SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
  context.Freeze();
  EXPECT_EQ(
      "{'I':{'z':["
      "[1,{'S':{'z':{'foo':{'X':{'q':0,'i':0,'x':2.0}}}}}],"
      "[3,{'S':{'z':{'bar':{'X':{'q':1,'i':1,'x':4.0}}}}}]"
      "]}}",
      SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
}

TEST(OptimizationVars, MultiDimensionalStringInt) {
  using namespace current::expression;
  VarsContext context;
  x["foo"][1] = 2;
  x["bar"][3] = 4;
  EXPECT_EQ("{'S':{'z':{'bar':{'I':{'z':[[3,{'X':{'q':1,'x':4.0}}]]}},'foo':{'I':{'z':[[1,{'X':{'q':0,'x':2.0}}]]}}}}}",
            SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
  context.Freeze();
  EXPECT_EQ(
      "{'S':{'z':{"
      "'bar':{'I':{'z':[[3,{'X':{'q':1,'i':0,'x':4.0}}]]}},"
      "'foo':{'I':{'z':[[1,{'X':{'q':0,'i':1,'x':2.0}}]]}}"
      "}}}",
      SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
}

TEST(OptimizationVars, Constants) {
  using namespace current::expression;
  VarsContext context;
  x["one"] = 1;
  x["two"] = 2;
  x["three"] = 3;
  EXPECT_EQ(
      "{'S':{'z':{"
      "'one':{'X':{'q':0,'x':1.0}},"
      "'three':{'X':{'q':2,'x':3.0}},"
      "'two':{'X':{'q':1,'x':2.0}}"
      "}}}",
      SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
  x["two"].SetConstant();
  x["three"].SetConstant(3.0);
  x["four"].SetConstant(4);
  ASSERT_THROW(x["one"].SetConstant(42), VarNodeReassignmentAttemptException);
  EXPECT_EQ(
      "{'S':{'z':{"
      "'four':{'X':{'q':3,'x':4.0,'c':true}},"
      "'one':{'X':{'q':0,'x':1.0}},"
      "'three':{'X':{'q':2,'x':3.0,'c':true}},"
      "'two':{'X':{'q':1,'x':2.0,'c':true}}"
      "}}}",
      SingleQuoted(JSON<JSONFormat::Minimalistic>(x.InternalDebugDump())));
}

TEST(OptimizationVars, DenseRepresentation) {
  using namespace current::expression;
  VarsContext context;
  x["x"]["x1"] = 101;  // Add the values in an arbitrary order to test they get sorted before being flattened.
  x["x"]["x3"] = 103;
  x["x"]["x2"] = 102;
  x["y"][0][0] = 200;
  x["y"][1][1] = 211;
  x["y"][0][1] = 201;
  x["y"][1][0] = 210;
  x["x"]["x2"].SetConstant();  // Make two of them constants for the test.
  x["y"][1][0].SetConstant();
  EXPECT_VAR_NAME_IS_RIGHT(x["x"]["x1"]);
  EXPECT_VAR_NAME_IS_RIGHT(x["x"]["x2"]);
  EXPECT_VAR_NAME_IS_RIGHT(x["x"]["x3"]);
  EXPECT_VAR_NAME_IS_RIGHT(x["y"][0][0]);
  EXPECT_VAR_NAME_IS_RIGHT(x["y"][0][1]);
  EXPECT_VAR_NAME_IS_RIGHT(x["y"][1][0]);
  EXPECT_VAR_NAME_IS_RIGHT(x["y"][1][1]);
  VarsMapperConfig const config = context.Freeze();
  ASSERT_EQ(7u, config.name.size());
  EXPECT_VAR_NAME_WITH_INDEX_IS_RIGHT(x["x"]["x1"], 0);
  EXPECT_VAR_NAME_WITH_INDEX_IS_RIGHT(x["x"]["x2"], 1);
  EXPECT_VAR_NAME_WITH_INDEX_IS_RIGHT(x["x"]["x3"], 2);
  EXPECT_VAR_NAME_WITH_INDEX_IS_RIGHT(x["y"][0][0], 3);
  EXPECT_VAR_NAME_WITH_INDEX_IS_RIGHT(x["y"][0][1], 4);
  EXPECT_VAR_NAME_WITH_INDEX_IS_RIGHT(x["y"][1][0], 5);
  EXPECT_VAR_NAME_WITH_INDEX_IS_RIGHT(x["y"][1][1], 6);
  EXPECT_EQ("x['x']['x1']{0}", SingleQuoted(config.name[0]));
  EXPECT_EQ("x['x']['x2']{1}", SingleQuoted(config.name[1]));
  EXPECT_EQ("x['x']['x3']{2}", SingleQuoted(config.name[2]));
  EXPECT_EQ("x['y'][0][0]{3}", SingleQuoted(config.name[3]));
  EXPECT_EQ("x['y'][0][1]{4}", SingleQuoted(config.name[4]));
  EXPECT_EQ("x['y'][1][0]{5}", SingleQuoted(config.name[5]));
  EXPECT_EQ("x['y'][1][1]{6}", SingleQuoted(config.name[6]));
  EXPECT_EQ("[101.0,102.0,103.0,200.0,201.0,210.0,211.0]", JSON(config.x0));
  EXPECT_EQ("[false,true,false,false,false,true,false]", JSON(config.is_constant));

  {
    VarsMapper a(config);
    VarsMapper b(config);  // To confirm no global or thread-local context is used by `VarsMapper`-s.

    EXPECT_EQ(JSON(a.x), JSON(config.x0));
    EXPECT_EQ(JSON(b.x), JSON(config.x0));

    EXPECT_EQ(101, a.x[0]);
    EXPECT_EQ(102, a.x[1]);
    EXPECT_EQ(211, a.x[6]);
    EXPECT_EQ(101, b.x[0]);
    EXPECT_EQ(102, b.x[1]);
    EXPECT_EQ(211, b.x[6]);

    a["x"]["x1"] = 70101;
    a["x"]["x2"].SetConstantValue(70102);
    a["y"][1][1] = 70211;

    b["x"]["x1"] = 80101;
    b["y"][1][1].Ref() = 80211;
    b["x"]["x2"].RefEvenForAConstant() = 80102;

    EXPECT_EQ(70101, a.x[0]);
    EXPECT_EQ(70102, a.x[1]);
    EXPECT_EQ(70211, a.x[6]);

    EXPECT_EQ(80101, b.x[0]);
    EXPECT_EQ(80102, b.x[1]);
    EXPECT_EQ(80211, b.x[6]);

    ASSERT_THROW(a[42] = 0, VarsMapperWrongVarException);
    ASSERT_THROW(a["z"] = 0, VarsMapperWrongVarException);
    ASSERT_THROW(a["x"][42] = 0, VarsMapperWrongVarException);
    ASSERT_THROW(a["x"]["x4"] = 0, VarsMapperWrongVarException);
    ASSERT_THROW(a["x"]["x1"]["foo"] = 0, VarsMapperWrongVarException);

    ASSERT_THROW(a["y"] = 0, VarsMapperNodeNotVarException);

    ASSERT_THROW(a["x"]["x2"].Ref(), VarsMapperVarIsConstant);
    ASSERT_THROW(a["x"]["x2"] = 0, VarsMapperVarIsConstant);
  }
}

TEST(OptimizationVars, DenseVectorDimensions) {
  using namespace current::expression;
  VarsContext context;
  ASSERT_THROW(x.DenseDoubleVector(0), VarsManagementException);
  ASSERT_THROW(x.DenseDoubleVector(static_cast<size_t>(1e6) + 1), VarsManagementException);
}

TEST(OptimizationVars, NeedContext) {
  using namespace current::expression;
  ASSERT_THROW(x["should fail"], VarsManagementException);
  ASSERT_THROW(x[42], VarsManagementException);
  ASSERT_THROW(x.DenseDoubleVector(1), VarsManagementException);
}

TEST(OptimizationVars, NoNestedContextsAllowed) {
  using namespace current::expression;
  VarsContext context;
  ASSERT_THROW(VarsContext illegal_inner_context, VarsManagementException);
}

#undef EXPECT_VAR_NAME_WITH_INDEX_IS_RIGHT
#undef EXPECT_VAR_NAME_IS_RIGHT
