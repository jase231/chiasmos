#pragma once

#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace chiasmos::output {

// reads one record, returning nullopt at end of input
[[nodiscard]] std::optional<std::vector<std::string>>
read_csv_record(std::istream& in);
void write_csv_record(std::ostream& out, std::span<std::string const> fields);

} // namespace chiasmos::output
