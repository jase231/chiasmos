#include "query/scoring.hpp"

#include <array>

namespace chiasmos::query {
namespace {

enum class DirMatch : std::uint8_t {
  Same,
  OtherPosition,
  QueryHasNone,
  RecordHasNone,
  Conflict
};

// match the directionals of a query's components and a FullRec
// handles single directionals eg. "N Main St"
// and double directionals in grid systems eg. "W 700 N"
DirMatch match_directionals(norm::Components const& q,
                            index::FullRec const& r) {
  using norm::Dir;
  Dir const qpre = q.pre_dir, qsuf = q.suf_dir,
            rpre = static_cast<Dir>(r.pre_dir),
            rsuf = static_cast<Dir>(r.suf_dir);
  int const nq = (qpre != Dir::None) + (qsuf != Dir::None),
            nr = (rpre != Dir::None) + (rsuf != Dir::None);
  if (nq == 0 || nr == 0)
    return nq == nr  ? DirMatch::Same
           : nq == 0 ? DirMatch::QueryHasNone
                     : DirMatch::RecordHasNone;
  if (nq == 2 && nr == 2)
    return qpre == rpre && qsuf == rsuf ? DirMatch::Same : DirMatch::Conflict;
  if (nq == 2 || nr == 2) {
    Dir const spre = nq == 1 ? qpre : rpre,
              ssuf = nq == 1 ? qsuf : rsuf; // the side with one directional
    Dir const ppre = nq == 2 ? qpre : rpre,
              psuf = nq == 2 ? qsuf : rsuf; // the side with two
    bool const single_is_prefix = spre != Dir::None;
    if ((single_is_prefix ? spre : ssuf) != (single_is_prefix ? ppre : psuf))
      return DirMatch::Conflict;
    return nq == 1
               ? DirMatch::QueryHasNone
               : DirMatch::RecordHasNone; // the side with only one says less
  }
  Dir const qd = qpre != Dir::None ? qpre : qsuf,
            rd = rpre != Dir::None ? rpre : rsuf;
  if (qd != rd)
    return DirMatch::Conflict;
  return (qpre != Dir::None) != (rpre != Dir::None) ? DirMatch::OtherPosition
                                                    : DirMatch::Same;
}

} // namespace

float name_agreement(norm::Components const& q, index::FullRec const& r,
                     bool numbered_street) {
  Scoring const& p = kScoring;
  float f = 1.0f;
  if (r.route == 0 && q.route == norm::RouteNet::None) {
    DirMatch const dm = match_directionals(q, r);
    if (numbered_street && (dm == DirMatch::Conflict ||
                            (q.type != 0 && r.type != 0 && q.type != r.type)))
      return 0.0f;
    std::array<float, 5> const dir_factor{ 1.0f, p.dir_other_position,
                                           p.dir_query_none, p.dir_record_none,
                                           p.dir_conflict }; // by DirMatch
    f *= dir_factor[static_cast<std::size_t>(dm)];
    if (q.type != r.type)
      f *= q.type == 0   ? p.type_query_none
           : r.type == 0 ? p.type_record_none
                         : p.type_conflict;
  }
  if (q.post_mod != r.post_mod)
    f *= p.mod_mismatch;
  return f;
}

} // namespace chiasmos::query
