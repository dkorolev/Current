/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev, <dmitry.korolev@gmail.com>.

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

// TODO(dkorolev): Handle empty POST body. Add a test for it.
// TODO(dkorolev): Support receiving body via POST requests. Add a test for it.

#ifndef BRICKS_NET_API_POSIX_H
#define BRICKS_NET_API_POSIX_H

#include "../types.h"
#include "../../url/url.h"

#include <memory>
#include <string>
#include <set>

#include "../../http/http.h"
#include "../../../file/file.h"

namespace bricks {
namespace net {
namespace api {

class HTTPClientPOSIX final {
 private:
  struct HTTPRedirectHelper : HTTPDefaultHelper {
    std::string location = "";
    inline void OnHeader(const char* key, const char* value) {
      if (std::string("Location") == key) {
        location = value;
      }
    }
  };
  typedef TemplatedHTTPReceivedMessage<HTTPRedirectHelper> HTTPRedirectableReceivedMessage;

 public:
  // The actual implementation.
  bool Go() {
    // TODO(dkorolev): Always use the URL returned by the server here.
    response_url_after_redirects_ = request_url_;
    url::URL parsed_url(request_url_);
    std::set<std::string> all_urls;
    bool redirected;
    do {
      redirected = false;
      const std::string composed_url = parsed_url.ComposeURL();
      if (all_urls.count(composed_url)) {
        throw HTTPRedirectLoopException();
      }
      all_urls.insert(composed_url);
      Connection connection(Connection(ClientSocket(parsed_url.host, parsed_url.port)));
      connection.BlockingWrite(request_method_ + ' ' + parsed_url.path + parsed_url.ComposeParameters() +
                               " HTTP/1.1\r\n");
      connection.BlockingWrite("Host: " + parsed_url.host + "\r\n");
      if (!request_user_agent_.empty()) {
        connection.BlockingWrite("User-Agent: " + request_user_agent_ + "\r\n");
      }
      if (!request_body_content_type_.empty()) {
        connection.BlockingWrite("Content-Type: " + request_body_content_type_ + "\r\n");
      }
      connection.BlockingWrite("Content-Length: " + std::to_string(request_body_contents_.length()) + "\r\n");
      connection.BlockingWrite("\r\n");
      connection.BlockingWrite(request_body_contents_);
      // Attention! Achtung! Увага! Внимание!
      // Calling SendEOF() (which is ::shutdown(socket, SHUT_WR);) results in slowly sent data
      // not being received. Tested on local and remote data with "chunked" transfer encoding.
      // Don't uncomment the next line!
      // connection.SendEOF();
      message_.reset(new HTTPRedirectableReceivedMessage(connection));
      // TODO(dkorolev): Rename `Path()`, it's only called so now because of HTTP request/response format.
      // Elaboration:
      // HTTP request  message is: `GET /path HTTP/1.1`, "/path" is the second component of it.
      // HTTP response message is: `HTTP/1.1 200 OK`, "200" is the second component of it.
      // Thus, since the same code is used for request and response parsing as of now,
      // the numerical response code "200" can be accessed with the same methos as the "/path".
      const int response_code_as_int = atoi(message_->Path().c_str());
      response_code_ = static_cast<HTTPResponseCode>(response_code_as_int);
      if (response_code_as_int >= 300 && response_code_as_int <= 399 && !message_->location.empty()) {
        // Note: This is by no means a complete redirect implementation.
        redirected = true;
        parsed_url = url::URL(message_->location, parsed_url);
        response_url_after_redirects_ = parsed_url.ComposeURL();
      }
    } while (redirected);
    return true;
  }

  const HTTPRedirectableReceivedMessage& GetMessage() const { return *message_.get(); }

 public:
  // Request parameters.
  std::string request_method_ = "";
  std::string request_url_ = "";
  std::string request_body_content_type_ = "";
  std::string request_body_contents_ = "";
  std::string request_user_agent_ = "";

  // Output parameters.
  HTTPResponseCode response_code_ = HTTPResponseCode::InvalidCode;
  std::string response_url_after_redirects_ = "";

 private:
  std::unique_ptr<HTTPRedirectableReceivedMessage> message_;
};

template <>
struct ImplWrapper<HTTPClientPOSIX> {
  inline static void PrepareInput(const GET& request, HTTPClientPOSIX& client) {
    client.request_method_ = "GET";
    client.request_url_ = request.url;
    if (!request.custom_user_agent.empty()) {
      client.request_user_agent_ = request.custom_user_agent;
    }
  }

  inline static void PrepareInput(const POST& request, HTTPClientPOSIX& client) {
    client.request_method_ = "POST";
    client.request_url_ = request.url;
    if (!request.custom_user_agent.empty()) {
      client.request_user_agent_ = request.custom_user_agent;  // LCOV_EXCL_LINE  -- tested in GET above.
    }
    client.request_body_contents_ = request.body;
    client.request_body_content_type_ = request.content_type;
  }

  inline static void PrepareInput(const POSTFromFile& request, HTTPClientPOSIX& client) {
    client.request_method_ = "POST";
    client.request_url_ = request.url;
    if (!request.custom_user_agent.empty()) {
      client.request_user_agent_ = request.custom_user_agent;  // LCOV_EXCL_LINE  -- tested in GET above.
    }
    client.request_body_contents_ =
        FileSystem::ReadFileAsString(request.file_name);  // Can throw FileException.
    client.request_body_content_type_ = request.content_type;
  }

  inline static void PrepareInput(const KeepResponseInMemory&, HTTPClientPOSIX&) {}

  inline static void PrepareInput(const SaveResponseToFile& save_to_file_request, HTTPClientPOSIX&) {
    assert(!save_to_file_request.file_name.empty());
  }

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& /*response_params*/,
                                 const HTTPClientPOSIX& response,
                                 HTTPResponse& output) {
    if (!request_params.allow_redirects && request_params.url != response.response_url_after_redirects_) {
      throw HTTPRedirectNotAllowedException();
    }
    output.url = response.response_url_after_redirects_;
    output.code = response.response_code_;
  }

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& response_params,
                                 const HTTPClientPOSIX& response,
                                 HTTPResponseWithBuffer& output) {
    ParseOutput(request_params, response_params, response, static_cast<HTTPResponse&>(output));
    const auto& message = response.GetMessage();
    output.body = message.HasBody() ? message.Body() : "";
  }

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& response_params,
                                 const HTTPClientPOSIX& response,
                                 HTTPResponseWithResultingFileName& output) {
    ParseOutput(request_params, response_params, response, static_cast<HTTPResponse&>(output));
    // TODO(dkorolev): This is doubly inefficient. Should write the buffer or write in chunks instead.
    const auto& message = response.GetMessage();
    FileSystem::WriteStringToFile(response_params.file_name.c_str(), message.HasBody() ? message.Body() : "");
    output.body_file_name = response_params.file_name;
  }
};

}  // namespace api
}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_API_POSIX_H
