# Bricks

The [`Bricks`](https://github.com/KnowSheet/Bricks/) repository contains core pieces to be reused across [`KnowSheet`](https://github.com/KnowSheet/) projects.

![](https://raw.githubusercontent.com/KnowSheet/Bricks/master/holy_bricks.jpg)

<sub>Image credit: Bing image search.</sub>

# Documentation

<sub>The following documentation has been auto-generated from source code by means of a [`gen-docu.sh`](https://github.com/KnowSheet/KnowSheet/blob/master/scripts/gen-docu.sh) script. Do not edit this file.</sub>

## Cerealize

Bricks uses [**Cereal**](http://uscilab.github.io/cereal/) for JSON and Binary serialization of C++ objects:

* [Cereal Website](http://uscilab.github.io/cereal/): Cereal is a header-only C++11 serialization library.
* [Cereal GitHub](https://github.com/USCiLab/cereal): A C++11 library for serialization.

<sub>Personal thanks for a well-designed C++11 serialization library! — @dkorolev</sub>

The [`#include "Bricks/cerealize/cerealize.h"`](https://github.com/KnowSheet/Bricks/blob/master/cerealize/cerealize.h) header makes the below code snippets complete.
```cpp
// Add a `serialize()` method to make a C++ structure "cerealizable".
struct SimpleType {
  int number;
  std::string string;
  std::vector<int> vector_int;
  std::map<int, std::string> map_int_string;
  template <typename A> void serialize(A& ar) {
    // Use `CEREAL_NVP(member)` to keep member names when using JSON.
    ar(CEREAL_NVP(number),
       CEREAL_NVP(string),
       CEREAL_NVP(vector_int),
       CEREAL_NVP(map_int_string));
  }
};
```
```cpp
// Use `JSON()` and `ParseJSON()` to create and parse JSON-s.
SimpleType x;
x.number = 42;
x.string = "test passed";
x.vector_int.push_back(1);
x.vector_int.push_back(2);
x.vector_int.push_back(3);
x.map_int_string[1] = "one";
x.map_int_string[42] = "the question";

// `JSON(object)` converts a cerealize-able object into a JSON string.
const std::string json = JSON(x);

// `ParseJSON<T>(json)` creates an instance of T from a JSON.
const SimpleType y = ParseJSON<SimpleType>(json);

// `ParseJSON(json, T& out)` allows omitting the type.
SimpleType z;
ParseJSON(json, z);
```
```cpp
// Use `load()/save()` instead of `serialize()` to customize serialization.
struct LoadSaveType {
  int a;
  int b;
  int sum;

  template <typename A> void save(A& ar) const {
    ar(CEREAL_NVP(a), CEREAL_NVP(b));
  }

  template <typename A> void load(A& ar) {
    ar(CEREAL_NVP(a), CEREAL_NVP(b));
    sum = a + b;
  }
};

LoadSaveType x;
x.a = 2;
x.b = 3;
EXPECT_EQ(5, ParseJSON<LoadSaveType>(JSON(x)).sum);
```
```cpp
// The example below uses `Printf()`, include it.
#include "strings/printf.h"
using bricks::strings::Printf;
 
// Polymorphic types are supported with some caution.
struct ExamplePolymorphicType {
  std::string base;
  explicit ExamplePolymorphicType(const std::string& base = "") : base(base) {}

  virtual std::string AsString() const = 0;
  template <typename A> void serialize(A& ar) const {
    ar(CEREAL_NVP(base));
  }
};

struct ExamplePolymorphicInt : ExamplePolymorphicType {
  int i;
  explicit ExamplePolymorphicInt(int i = 0)
      : ExamplePolymorphicType("int"), i(i) {}

  virtual std::string AsString() const override {
    return Printf("%s, %d", base.c_str(), i);
  }

  template <typename A> void serialize(A& ar) const {
    ExamplePolymorphicType::serialize(ar);
    ar(CEREAL_NVP(i));
  }
};
// Need to register the derived type.
CEREAL_REGISTER_TYPE(ExamplePolymorphicInt);

struct ExamplePolymorphicDouble : ExamplePolymorphicType {
  double d;
  explicit ExamplePolymorphicDouble(double d = 0)
      : ExamplePolymorphicType("double"), d(d) {}

  virtual std::string AsString() const override {
    return Printf("%s, %lf", base.c_str(), d);
  }

  template <typename A> void serialize(A& ar) const {
    ExamplePolymorphicType::serialize(ar);
    ar(CEREAL_NVP(d));
  }
};
// Need to register the derived type.
CEREAL_REGISTER_TYPE(ExamplePolymorphicDouble);

const std::string json_int =
  JSON(WithBaseType<ExamplePolymorphicType>(ExamplePolymorphicInt(42)));

const std::string json_double =
  JSON(WithBaseType<ExamplePolymorphicType>(ExamplePolymorphicDouble(M_PI)));

EXPECT_EQ("int, 42",
          ParseJSON<std::unique_ptr<ExamplePolymorphicType>>(json_int)->AsString());

EXPECT_EQ("double, 3.141593",
          ParseJSON<std::unique_ptr<ExamplePolymorphicType>>(json_double)->AsString());
```
## REST API Toolkit

The [`#include "Bricks/net/api/api.h"`](https://github.com/KnowSheet/Bricks/blob/master/net/api/api.h) header enables to run the code snippets below.

### HTTP Client

```cpp
// Simple GET.
EXPECT_EQ("OK", HTTP(GET("test.tailproduce.org/ok")).body);

// More fields.
const auto response = HTTP(GET("test.tailproduce.org/ok"));
EXPECT_EQ("OK", response.body);
EXPECT_TRUE(response.code == HTTPResponseCode.OK);
```
```cpp
// POST is supported as well.
EXPECT_EQ("OK", HTTP(POST("test.tailproduce.org/ok"), "BODY", "text/plain").body);

// Beyond plain strings, cerealizable objects can be passed in.
// JSON will be sent, as "application/json" content type.
EXPECT_EQ("OK", HTTP(POST("test.tailproduce.org/ok"), SimpleType()).body);

```
HTTP client supports headers, POST-ing data to and from files, and many other features as well. Check the unit test in [`bricks/net/api/test.cc`](https://github.com/KnowSheet/Bricks/blob/master/net/api/test.cc) for more details.
### HTTP Server
```cpp
// Simple "OK" endpoint.
HTTP(port).Register("/ok", [](Request r) {
  r("OK");
});
```
```cpp
// Accessing input fields.
HTTP(port).Register("/demo", [](Request r) {
  r(r.url.query["q"] + ' ' + r.method + ' ' + r.body);
});
```
```cpp
// Constructing a more complex response.
HTTP(port).Register("/found", [](Request r) {
  r("Yes.",
    HTTPResponseCode.Accepted,
    "text/html",
    HTTPHeaders().Set("custom", "header").Set("another", "one"));
});
```
```cpp
// An input record that would be passed in as a JSON.
struct PennyInput {
  std::string op;
  std::vector<int> x;
  std::string error;  // Not serialized.
  template <typename A> void serialize(A& ar) {
    ar(CEREAL_NVP(op), CEREAL_NVP(x));
  }
  void FromInvalidJSON(const std::string& input_json) {
    error = "JSON parse error: " + input_json;
  }
};

// An output record that would be sent back as a JSON.
struct PennyOutput {
  std::string error;
  int result;
  template <typename A> void serialize(A& ar) {
    ar(CEREAL_NVP(error), CEREAL_NVP(result));
  }
}; 

// Doing Penny-level arithmetics for fun and performance testing.
HTTP(port).Register("/penny", [](Request r) {
  const auto input = ParseJSON<PennyInput>(r.body);
  if (!input.error.empty()) {
    r(PennyOutput{input.error, 0});
  } else {
    if (input.op == "add") {
      if (!input.x.empty()) {
        int result = 0;
        for (const auto v : input.x) {
          result += v;
        }
        r(PennyOutput{"", result});
      } else {
        r(PennyOutput{"Not enough arguments for 'add'.", 0});
      }
    } else if (input.op == "mul") {
      if (!input.x.empty()) {
        int result = 1;
        for (const auto v : input.x) {
          result *= v;
        }
        r(PennyOutput{"", result});
      } else {
        r(PennyOutput{"Not enough arguments for 'mul'.", 0});
      }
    } else {
      r(PennyOutput{"Unknown operation: " + input.op, 0});
    }
  }
});
```
HTTP server also has support for several other features, check out the [`bricks/net/api/test.cc`](https://github.com/KnowSheet/Bricks/blob/master/net/api/test.cc) unit test.

**TODO(dkorolev)**: Chunked response example, with a note that it goes to Sherlock.
## Visualization Library

Bricks has C++ bindings for [`plotutils`](http://www.gnu.org/software/plotutils/) and [`gnuplot`](http://www.gnuplot.info/). Use [`#include "Bricks/graph/plotutils.h"`](https://github.com/KnowSheet/Bricks/blob/master/graph/plotutils.h) and [`#include "Bricks/graph/gnuplot.h"`](https://github.com/KnowSheet/Bricks/blob/master/graph/gnuplot.h).

The [`plotutils`](http://www.gnu.org/software/plotutils/) tool is somewhat simpler and lighter. The [`gnuplot`](http://www.gnuplot.info/) one is more scientific and offers a wider range of features.

Both libraries are invoked as external system calls. Make sure they are installed in the system that runs your code.
### Using `plotutils`
```cpp
// Where visualization meets love.
const size_t N = 1000;
std::vector<std::pair<double, double>> line(N);
  
for (size_t i = 0; i < N; ++i) {
  const double t = M_PI * 2 * i / (N - 1);
  line[i] = std::make_pair(
    16 * pow(sin(t), 3),
    -(13 * cos(t) + 5 * cos(t * 2) - 2 * cos(t * 3) - cos(t * 4)));
}

// Pull Plotutils, LineColor, GridStyle and more plotutils-related symbols.
using namespace bricks::plotutils;

const std::string result = Plotutils(line)
  .LineMode(CustomLineMode(LineColor::Red, LineStyle::LongDashed))
  .GridStyle(GridStyle::Full)
  .Label("Imagine all the people ...")
  .X("... living life in peace")
  .Y("John Lennon, \"Imagine\"")
  .LineWidth(0.015)
  .OutputFormat("svg");
```
![](https://raw.githubusercontent.com/dkorolev/Bricks/png/graph/golden/love.png)
### Using `gnuplot`
```cpp
// Where visualization meets science.
using namespace bricks::gnuplot;
const std::string result = GNUPlot()
  .Title("Foo 'bar' \"baz\"")
  .KeyTitle("Meh 'in' \"quotes\"")
  .XRange(-42, 42)
  .YRange(-2.5, +2.5)
  .Grid("back")
  .Plot([](Plotter& p) {
    for (int i = -100; i <= +100; ++i) {
      p(i, ::sin(0.1 * i));
    }
  })
  .Plot(WithMeta([](Plotter& p) {
                   for (int i = -100; i <= +100; ++i) {
                     p(i, ::cos(0.1 * i));
                   }
                 })
            .Name("\"Cosine\" as 'points'")
            .AsPoints())
  .OutputFormat("svg");
```
![](https://raw.githubusercontent.com/dkorolev/Bricks/png/graph/golden/gnuplot.png)
## Run-Time Type Dispatching

Bricks can dispatch calls to the right implementation at runtime, with user code being free of virtual functions.

This comes especially handy when processing log entries from a large stream of data, where only a few types are of immediate interest.

Use the [`#include "Bricks/rtti/dispatcher.h"`](https://github.com/KnowSheet/Bricks/blob/master/rtti/dispatcher.h) header to run the code snippets below.

`TODO(dkorolev)` a wiser way for the end user to leverage the above is by means of `Sherlock` once it's checked in.
```cpp
// The example below uses `Printf()`, include it.
#include "strings/printf.h"
using bricks::strings::Printf;
 
struct ExampleBase {
  virtual ~ExampleBase() = default;
};

struct ExampleInt : ExampleBase {
  int i;
  explicit ExampleInt(int i) : i(i) {}
};

struct ExampleString : ExampleBase {
  std::string s;
  explicit ExampleString(const std::string& s) : s(s) {}
};

struct ExampleMoo : ExampleBase {
};

struct ExampleProcessor {
  std::string result;
  void operator()(const ExampleBase&) { result = "unknown"; }
  void operator()(const ExampleInt& x) { result = Printf("int %d", x.i); }
  void operator()(const ExampleString& x) { result = Printf("string '%s'", x.s.c_str()); }
  void operator()(const ExampleMoo&) { result = "moo!"; }
};

using bricks::rtti::RuntimeTupleDispatcher;
typedef RuntimeTupleDispatcher<ExampleBase,
                               tuple<ExampleInt, ExampleString, ExampleMoo>> Dispatcher;

ExampleProcessor processor;

Dispatcher::DispatchCall(ExampleBase(), processor);
EXPECT_EQ(processor.result, "unknown");

Dispatcher::DispatchCall(ExampleInt(42), processor);
EXPECT_EQ(processor.result, "int 42");

Dispatcher::DispatchCall(ExampleString("foo"), processor);
EXPECT_EQ(processor.result, "string 'foo'");

Dispatcher::DispatchCall(ExampleMoo(), processor);
EXPECT_EQ(processor.result, "moo!");
```
## Extras

[`Bricks`](https://github.com/KnowSheet/Bricks/) contains a few other useful bits, such as threading primitives, in-memory message queue and system clock utilities.
