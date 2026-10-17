#include "server/server.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>

#include "output/geojson.hpp"
#include "output/params.hpp"

namespace chiasmos::server {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace {

http::response<http::string_body>
reply(http::request<http::string_body> const& req, http::status status,
      std::string body, std::string_view type) {
  http::response<http::string_body> res{ status, req.version() };
  res.set(http::field::server, "chiasmos");
  res.set(http::field::content_type, type);
  res.keep_alive(req.keep_alive());
  res.body() = std::move(body);
  res.prepare_payload();
  return res;
}

http::response<http::string_body>
handle(query::Engine const& engine,
       http::request<http::string_body> const& req) {
  auto const error = [&](http::status s, std::string_view msg) {
    return reply(req, s, nlohmann::json{ { "error", msg } }.dump(),
                 "application/json");
  };
  if (req.method() != http::verb::get)
    return error(http::status::method_not_allowed, "GET only");
  std::string_view const target = req.target();
  auto const qmark = target.find('?');
  if (target.substr(0, qmark) != "/v1/intersections")
    return error(http::status::not_found,
                 "try GET /v1/intersections?q=Main+St+%26+1st+Ave");
  auto const parsed = output::request_from_query_string(
      qmark == std::string_view::npos ? "" : target.substr(qmark + 1));
  if (!parsed)
    return error(http::status::bad_request, parsed.error().msg);
  query::Result const result = engine.run(*parsed);
  return reply(req, http::status::ok,
               output::to_geojson(*parsed, result).dump(),
               "application/geo+json");
}

// serves one connection until the client closes
void session(tcp::socket& socket, query::Engine const& engine) {
  beast::flat_buffer buffer;
  for (beast::error_code ec;;) {
    http::request<http::string_body> req;
    http::read(socket, buffer, req, ec);
    if (ec)
      return;
    auto const res = handle(engine, req);
    http::write(socket, res, ec);
    if (ec || !res.keep_alive())
      break;
  }
  beast::error_code ignored;
  ignored = socket.shutdown(tcp::socket::shutdown_send, ignored);
}

} // namespace

Result<void> serve(query::Engine const& engine, std::string const& host,
                   std::uint16_t port) {
  beast::error_code ec;
  auto const address = net::ip::make_address(host, ec);
  if (ec)
    return fail(Errc::InvalidArgument,
                "bad listen address '" + host + "': " + ec.message());
  tcp::endpoint const endpoint{ address, port };
  net::io_context io;
  tcp::acceptor acceptor(io);
  if (acceptor.open(endpoint.protocol(), ec); ec)
    return fail(Errc::Io, ec.message());
  if (acceptor.set_option(net::socket_base::reuse_address(true), ec); ec)
    return fail(Errc::Io, ec.message());
  if (acceptor.bind(endpoint, ec); ec)
    return fail(Errc::Io, "bind " + host + ": " + ec.message());
  if (acceptor.listen(net::socket_base::max_listen_connections, ec); ec)
    return fail(Errc::Io, ec.message());
  for (;;) {
    tcp::socket socket = acceptor.accept(ec);
    if (!ec)
      session(socket, engine);
  }
}

} // namespace chiasmos::server
