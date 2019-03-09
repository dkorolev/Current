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

// TODO(dkorolev): Locking and immutability.
// TODO(dkorolev): Index enumeration.
// TODO(dkorolev): Two separate "vars" universes, for the variables and for the constants.

#include "vars.h"

#include "../../3rdparty/gtest/gtest-main.h"

inline std::string SingleQuoted(std::string s) {
  for (char& c : s) {
    if (c == '\"') {
      c = '\'';
    }
  }
  return s;
}

TEST(OptimizationVars, SparseByInt) {
  using namespace current::expression;
  VarsContext context;
  c[1] = 2;
  c[100] = 101;
  c[42] = 0;
  // The indexes should be sorted. -- D.K.
  EXPECT_EQ("{'I':{'z':[[1,{'X':{'i':0,'x':2.0}}],[42,{'X':{'i':2,'x':0.0}}],[100,{'X':{'i':1,'x':101.0}}]]}}",
            SingleQuoted(JSON<JSONFormat::Minimalistic>(c.Dump())));
  ASSERT_THROW(c.DenseDoubleVector(100), current::expression::VarsManagementException);
  ASSERT_THROW(c["foo"], current::expression::VarsManagementException);
  ASSERT_THROW(c[1][2], current::expression::VarsManagementException);
  ASSERT_THROW(c[1]["blah"], current::expression::VarsManagementException);
  ASSERT_THROW(c[1].DenseDoubleVector(100), current::expression::VarsManagementException);
}

TEST(OptimizationVars, SparseByString) {
  using namespace current::expression;
  VarsContext context;
  c["foo"] = 1;
  c["bar"] = 2;
  c["baz"] = 3;
  // The string "indexes" should be sorted. -- D.K.
  EXPECT_EQ("{'S':{'z':{'bar':{'X':{'i':1,'x':2.0}},'baz':{'X':{'i':2,'x':3.0}},'foo':{'X':{'i':0,'x':1.0}}}}}",
            SingleQuoted(JSON<JSONFormat::Minimalistic>(c.Dump())));
  ASSERT_THROW(c.DenseDoubleVector(100), current::expression::VarsManagementException);
  ASSERT_THROW(c[42], current::expression::VarsManagementException);
  ASSERT_THROW(c["foo"][2], current::expression::VarsManagementException);
  ASSERT_THROW(c["foo"]["blah"], current::expression::VarsManagementException);
  ASSERT_THROW(c["foo"].DenseDoubleVector(100), current::expression::VarsManagementException);
}

TEST(OptimizationVars, DenseVector) {
  using namespace current::expression;
  VarsContext context;
  c.DenseDoubleVector(5);
  c[2] = 2;
  c[4] = 4;
  EXPECT_EQ("{'V':{'z':[{'U':{}},{'U':{}},{'X':{'i':0,'x':2.0}},{'U':{}},{'X':{'i':1,'x':4.0}}]}}",
            SingleQuoted(JSON<JSONFormat::Minimalistic>(c.Dump())));
  ASSERT_THROW(c[42], current::expression::VarsManagementException);
  ASSERT_THROW(c["foo"], current::expression::VarsManagementException);
  c.DenseDoubleVector(5);  // Same size, a valid no-op.
  ASSERT_THROW(c.DenseDoubleVector(100), current::expression::VarsManagementException);
}

TEST(OptimizationVars, LockingDownDeathTest) {
  using namespace current::expression;
  VarsContext context;
  c["dense"].DenseDoubleVector(2);
  c["sparse"][42] = 42;
  c["strings"]["foo"] = 1;
  c.Lock();
  c["dense"][0];
  c["dense"][1];
  c["sparse"][42];
  c["strings"]["foo"];
  ASSERT_THROW(c["dense"][2], current::expression::VarsManagementException);
  ASSERT_THROW(c["sparse"][100], current::expression::VarsManagementException);
  ASSERT_THROW(c["strings"]["bar"], current::expression::VarsManagementException);
  ASSERT_THROW(c["foo"], current::expression::VarsManagementException);
}

TEST(OptimizationVars, MultiDimensionalIntInt) {
  using namespace current::expression;
  VarsContext context;
  c[1][2] = 3;
  c[4][5] = 6;
  EXPECT_EQ("{'I':{'z':[[1,{'I':{'z':[[2,{'X':{'i':0,'x':3.0}}]]}}],[4,{'I':{'z':[[5,{'X':{'i':1,'x':6.0}}]]}}]]}}",
            SingleQuoted(JSON<JSONFormat::Minimalistic>(c.Dump())));
}

TEST(OptimizationVars, MultiDimensionalIntString) {
  using namespace current::expression;
  VarsContext context;
  c[1]["foo"] = 2;
  c[3]["bar"] = 4;
  EXPECT_EQ("{'I':{'z':[[1,{'S':{'z':{'foo':{'X':{'i':0,'x':2.0}}}}}],[3,{'S':{'z':{'bar':{'X':{'i':1,'x':4.0}}}}}]]}}",
            SingleQuoted(JSON<JSONFormat::Minimalistic>(c.Dump())));
}

TEST(OptimizationVars, MultiDimensionalStringInt) {
  using namespace current::expression;
  VarsContext context;
  c["foo"][1] = 2;
  c["bar"][3] = 4;
  EXPECT_EQ("{'S':{'z':{'bar':{'I':{'z':[[3,{'X':{'i':1,'x':4.0}}]]}},'foo':{'I':{'z':[[1,{'X':{'i':0,'x':2.0}}]]}}}}}",
            SingleQuoted(JSON<JSONFormat::Minimalistic>(c.Dump())));
}

TEST(OptimizationVars, DenseVectorDimensionsDeathTest) {
  using namespace current::expression;
  VarsContext context;
  ASSERT_THROW(c.DenseDoubleVector(0), VarsManagementException);
  ASSERT_THROW(c.DenseDoubleVector(static_cast<size_t>(1e6) + 1), VarsManagementException);
}

TEST(OptimizationVars, NeedContextDeathTest) {
  using namespace current::expression;
  ASSERT_THROW(c["should fail"], VarsManagementException);
  ASSERT_THROW(c[42], VarsManagementException);
  ASSERT_THROW(c.DenseDoubleVector(1), VarsManagementException);
}

TEST(OptimizationVars, NoNestedContextsAllowedDeathTest) {
  using namespace current::expression;
  VarsContext context;
  ASSERT_THROW(VarsContext illegal_inner_context, VarsManagementException);
}
