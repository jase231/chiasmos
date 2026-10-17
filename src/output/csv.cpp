#include "output/csv.hpp"

#include <istream>
#include <ostream>
#include <utility>

namespace chiasmos::output {

std::optional<std::vector<std::string>> read_csv_record(std::istream& in) {
  if (in.peek() == std::istream::traits_type::eof())
    return std::nullopt;
  if (in.tellg() == 0 && in.peek() == 0xEF) { // UTF-8 BOM: EF BB BF
    char bom[3];
    in.read(bom, 3);
  }
  std::vector<std::string> record(1);
  bool quoted = false;
  for (char c; in.get(c);) {
    std::string& field = record.back();
    if (quoted) {
      if (c != '"')
        field += c;
      else if (in.peek() == '"')
        field += static_cast<char>(in.get());
      else
        quoted = false;
    } else if (c == '"')
      quoted = true;
    else if (c == ',')
      record.emplace_back();
    else if (c == '\n')
      break;
    else if (c != '\r')
      field += c;
  }
  return record;
}

void write_csv_record(std::ostream& out, std::span<std::string const> fields) {
  bool first = true;
  for (std::string const& f : fields) {
    if (!std::exchange(first, false))
      out << ',';
    if (f.find_first_of(",\"\r\n") == std::string::npos) {
      out << f;
      continue;
    }
    out << '"';
    for (char const c : f)
      out << (c == '"' ? "\"\"" : std::string(1, c));
    out << '"';
  }
  out << '\n';
}

} // namespace chiasmos::output
