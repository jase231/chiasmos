#pragma once

#include <filesystem>
#include <string>

namespace chiasmos::cli {

struct BuildOptions {
  std::filesystem::path input, output;
  std::string region;
  bool include_paths = false;
};

struct QueryOptions {
  std::filesystem::path index;
  std::string q, street1, street2, near, bbox;
  int limit = 5;
  float min_score = 0.15f;
  bool pretty = false;
};

struct BatchOptions {
  std::filesystem::path index, in, out;
  std::string q_col, near, bbox; // near and bbox apply to every row
  int limit = 3;
};

struct ServeOptions {
  std::filesystem::path index;
  std::string listen = "127.0.0.1:8080";
};

struct InspectOptions {
  std::filesystem::path index;
  bool verify = false;
};

int run_build(BuildOptions const& o);
int run_query(QueryOptions const& o);
int run_batch(BatchOptions const& o);
int run_serve(ServeOptions const& o);
int run_inspect(InspectOptions const& o);

} // namespace chiasmos::cli
