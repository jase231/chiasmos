#include "server/server.hpp"

#include <csignal>
#include <exception>
#include <future>

#include <beauty/beauty.hpp>
#include <boost/asio/signal_set.hpp>
#include <nlohmann/json.hpp>

#include "output/geojson.hpp"
#include "output/params.hpp"

namespace chiasmos::server {

namespace {

void reply(beauty::response& res, beauty::http::status status, std::string body,
           std::string_view type) {
  res.result(status);
  res.set(beauty::http::field::server, "chiasmos");
  res.set(beauty::http::field::content_type, type);
  res.body() = std::move(body);
}

void error(beauty::response& res, beauty::http::status status,
           std::string_view msg) {
  reply(res, status, nlohmann::json{ { "error", msg } }.dump(),
        "application/json");
}

void intersections(query::Engine const& engine, beauty::request const& req,
                   beauty::response& res) {
  std::string_view const target = req.target();
  auto const qmark = target.find('?');
  auto const parsed = output::request_from_query_string(
      qmark == std::string_view::npos ? "" : target.substr(qmark + 1));
  if (!parsed)
    return error(res, beauty::http::status::bad_request, parsed.error().msg);
  query::Result const result = engine.run(*parsed);
  reply(res, beauty::http::status::ok,
        output::to_geojson(*parsed, result).dump(), "application/geo+json");
}

} // namespace

Result<void> serve(query::Engine const& engine, Options const& opts) {
  beauty::application app;
  beauty::server server(app);
  server.concurrency(opts.threads);

  auto const not_allowed = [](beauty::request const&, beauty::response& res) {
    error(res, beauty::http::status::method_not_allowed, "GET only");
  };
  server.add_route("/v1/intersections")
      .get([&engine](beauty::request const& req, beauty::response& res) {
        intersections(engine, req, res);
      })
      .post(not_allowed)
      .put(not_allowed)
      .patch(not_allowed)
      .del(not_allowed);

  try {
    server.listen(opts.port, opts.host);
  } catch (std::exception const& e) {
    return fail(Errc::InvalidArgument,
                "bad listen address '" + opts.host + "': " + e.what());
  }

  // beauty doesn't throw on failing to listen/bind
  if (app.is_stopped())
    return fail(Errc::Io, "could not listen on " + opts.host + ":" +
                              std::to_string(opts.port));

  // stop from main thread
  std::promise<void> stop;
  boost::asio::signal_set signals(app.ioc(), SIGINT, SIGTERM);
  signals.async_wait([&stop](auto const&, int /* ignore asio params*/) { stop.set_value(); });
  stop.get_future().wait();
  server.stop();
  return {};
}

} // namespace chiasmos::server
