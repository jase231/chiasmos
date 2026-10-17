#pragma once
// Public query API: two street names (or free-form text) in, ranked
// intersections out.
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/types.hpp"
#include "index/index.hpp"

namespace chiasmos::query {

struct Request {
  // structured input; the two streets are symmetric
  std::string street1, street2;
  // free-form input used when street1 and street2 are both empty
  std::string q;
  // preference for candidates near a certain coordinate
  std::optional<Coord> near;
  // hard filter on a junction's representative point
  std::optional<Bbox> bbox;
  // at most this many candidates
  int limit = 5;
  // candidates scoring below this are dropped
  float min_score = 0.15f;
};

enum class Confidence : std::uint8_t { High, Medium, Low };
[[nodiscard]] constexpr std::string_view to_string(Confidence c) {
  return c == Confidence::High     ? "high"
         : c == Confidence::Medium ? "medium"
                                   : "low";
}

struct Candidate {
  Coord rep; // weighted center for merges
  float score = 0;
  Confidence confidence = Confidence::Low;
  JunctionKind kind = JunctionKind::Node;
  std::string street1, street2; // display names, in request's format
  std::vector<std::int64_t> osm_nodes;
  float distance_to_near_m = -1; // only used if Request::near was specified
};

struct Result {
  Status status = Status::InvalidRequest;
  // best first, size at most Request::limit
  std::vector<Candidate> candidates;
  // human readable description of Status details
  std::vector<std::string> notes;
};

class Engine {
public:
  explicit Engine(index::Index const& ix) : ix_(&ix) {}
  [[nodiscard]] Result run(Request const& req) const;

private:
  index::Index const* ix_;
};

} // namespace chiasmos::query
