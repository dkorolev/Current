## REST API Toolkit

The [`#include "blocks/http/api.h"`](https://github.com/C5T/Current/blob/stable/blocks/http/api.h) header enables to run the code snippets below.

### HTTP Client

```cpp
// Simple GET.
EXPECT_EQ("OK", HTTP(GET("http://test.tailproduce.org/ok")).body);

// More fields.
const auto response = HTTP(GET("http://test.tailproduce.org/ok"));
EXPECT_EQ("OK", response.body);
EXPECT_TRUE(response.code == HTTPResponseCode.OK);
```
```cpp
CURRENT_FIELD(s, std::string);
// POST is supported as well.
EXPECT_EQ("OK", HTTP(POST("http://test.tailproduce.org/ok"), "BODY", "text/plain").body);

// Beyond plain strings, cerealizable objects can be passed in.
// JSON will be sent, as "application/json" content type.
EXPECT_EQ("OK", HTTP(POST("http://test.tailproduce.org/ok"), SimpleType()).body);

```
HTTP client supports headers, POST-ing data to and from files, and many other features as well. Check the unit test in [`Blocks/http/test.cc`](https://github.com/C5T/Current/blob/stable/blocks/http/test.cc) for more details.
### HTTP Server
```cpp
// Simple "OK" endpoint.
const auto scope = HTTP(port).Register("/ok", [](Request r) {
  r("OK");
});
```
```cpp
// Accessing input fields.
const auto scope = HTTP(port).Register("/demo", [](Request r) {
  r(r.url.query["q"] + ' ' + r.method + ' ' + r.body);
});
```
```cpp
// Constructing a more complex response.
const auto scope = HTTP(port).Register("/found", [](Request r) {
  r("Yes.",
    HTTPResponseCode.Accepted,
    current::net::constants::kDefaultHTMLContentType,
    current::net::http::Headers().Set("custom", "header").Set("another", "one"));
});
```
```cpp
// An input record that would be passed in as a JSON.
CURRENT_STRUCT(PennyInput) {
  CURRENT_FIELD(op, std::string);
  CURRENT_FIELD(x, std::vector<int32_t>);
  CURRENT_DEFAULT_CONSTRUCTOR(PennyInput) {}
  CURRENT_CONSTRUCTOR(PennyInput)(const std::string& op, const std::vector<int32_t>& x) : op(op), x(x) {}
};

// An output record that would be sent back as a JSON.
CURRENT_STRUCT(PennyOutput) {
  CURRENT_FIELD(error, std::string);
  CURRENT_FIELD(result, int32_t);
  CURRENT_CONSTRUCTOR(PennyOutput)(const std::string& error, int32_t result) : error(error), result(result) {}
}; 

// Doing Penny-level arithmetics for fun and performance testing.
const auto scope = HTTP(port).Register("/penny", [](Request r) {
  try {
    const auto input = ParseJSON<PennyInput>(r.body);
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
  } catch (const current::Exception& e) {
    // TODO(dkorolev): Catch the right exception type.
    r(PennyOutput{e.OriginalDescription(), 0});
  }
});
```
```cpp
// Returning a potentially unlimited response chunk by chunk.
const auto scope = HTTP(port).Register("/chunked", [](Request r) {
  const size_t n = atoi(r.url.query["n"].c_str());
  const size_t delay_ms = atoi(r.url.query["delay_ms"].c_str());
    
  const auto sleep = [&delay_ms]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
  };
    
  auto response = r.SendChunkedResponse();

  sleep();
  for (size_t i = 0; n && i < n; ++i) {
    response(".");  // Use double quotes for a string, not single quotes for a char.
    sleep();
  }
  response("\n");
});
 
EXPECT_EQ(".....\n", HTTP(GET("http://test.tailproduce.org/chunked")).body);
 
// NOTE: For most legitimate practical usecases of returning unlimited
// amounts of data, consider Stream's stream data replication mechanisms.
```
HTTP server also has support for several other features, check out the [`Blocks/http/test.cc`](https://github.com/C5T/Current/blob/stable/blocks/http/test.cc) unit test.
