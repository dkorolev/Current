#pragma once

#include "javascript_any.hpp"
#include "javascript_async.hpp"
#include "javascript_async_eventloop.hpp"
#include "javascript_cpp2js.hpp"
#include "javascript_env.hpp"
#include "javascript_function.hpp"
#include "javascript_function_cont.hpp"
#include "javascript_js2cpp.hpp"
#include "javascript_object.hpp"
#include "javascript_promise.hpp"

namespace current {
namespace javascript {

using impl::CPP2JS;
using impl::JS2CPP;
using impl::Undefined;

using impl::JSAny;
using impl::JSObject;

using impl::JSFunction;
using impl::JSFunctionReturning;

using impl::JSFunctionReference;
using impl::JSFunctionReferenceReturning;

using impl::JSAsync;

using impl::JSAsyncEventLoop;
using impl::JSAsyncEventLoopRunner;

using impl::JSPromise;

using impl::JSEnvScope;

}  // namespace javascript
}  // namespace current
