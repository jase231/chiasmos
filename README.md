# chiasmos
chiasmos is a fast forward geocoder for intersections using OpenStreetMap data.
Intersections are a blind spot for the OSM ecosystem's geocoding solution, Nominatim.
chiasmos serves an API supporting queries in two formats:
- A pair of street names, supplied as a parameters `street1` and `street2`
- A query string, with the intersection declared using one of the supported separators: &, @, /, --, and xst (cross street)

chiasmos depends heavily on a custom normalizer. That normalizer works well and has been tested for Virginia's road naming.
However, while other states have been tested, chiasmos' accuracy numbers in Virginia should not yet be extrapolated to other regions in the US.

# Demo
A web demo utilizing chiasmos is available at [https://chiasmos.serwe.dev/](https://chiasmos.serwe.dev/), and the public API is available at: [https://chiasmos.serwe.dev/v1/intersections](https://chiasmos.serwe.dev/v1/intersections).
The demo API's index is built from a Virginia extract, and can therefore only service Virginia queries.
The demo API This software is provided "as is" and "as available," without warranty of any kind, express or implied, including but not limited to the implied warranties of merchantability, fitness for a particular purpose, non-infringement, and data accuracy.

# Build
Needs a C++23 compiler, CMake 3.28+, Ninja, libosmium, protozero, zlib, CLI11, nlohmann_json, Boost 1.80+ with Boost.JSON (for `serve`ing the API; [beauty](https://github.com/dfleury2/beauty) is fetched at configure time; skip both with `-DCHIASMOS_SERVER=OFF`) and GoogleTest (skip with `-DCHIASMOS_TESTS=OFF`).
```sh
cmake --preset release && cmake --build --preset release   # binary: build/release/src/cli/chiasmos
```

# Usage
Build an index from an OSM extract (use Geofabrik's; every node a way uses must be in the file):
```sh
chiasmos build -i virginia-latest.osm.pbf -o virginia.cix --region us/virginia
```
Queries: `--near LAT,LON` or `--bbox WEST,SOUTH,EAST,NORTH` keeps same-named streets elsewhere out of the results:
```sh
chiasmos query -i virginia.cix --q "W Broad St & N Belvidere St" --near 37.55,-77.45
chiasmos query -i virginia.cix --street1 "W Broad St" --street2 "N Belvidere St" --bbox -77.60,37.45,-77.38,37.61
chiasmos batch -i virginia.cix --in in.csv --out out.csv --q-col address
chiasmos serve -i virginia.cix --host 127.0.0.1 --port 8080
curl '127.0.0.1:8080/v1/intersections?q=W+Broad+St+%26+N+Belvidere+St&near=37.55,-77.45'
```
Results are GeoJSON. `--limit` and `--min-score` (API: `limit`, `min_score`) cap and filter them. `chiasmos <command> --help` lists every option.

# Performance
chiasmos uses a custom pre-built index that enables very fast lookups.
When using the API, median and P99 response time in a test (720 cases from 8 Virginia counties, with ground truth from TIGER) were 0.12 and 0.15 ms.
If using batch mode (i.e. isolating the engine's performance from web overhead), a single geocode takes single digit microseconds.
Comparison to alternative techniques:
- querying the Overpass API for shared nodes over the same test data takes 61/131 ms (median, P99)
- implementing [this blog's technique](https://www.r-bloggers.com/2020/08/geocoding-an-intersection-with-open-street-map-data/) over the same data takes 322/1,842 ms (median, P99)

Intersection geocoding is inherently fuzzy, and some queries will fail. chiasmos quantifies its uncertainty about a given result with a score.
In the above test, chiasmos scored 87.7% accuracy against reference data, compared to 80.8% and 68.2% for the Overpass and blog's approaches.

# Terminology
(A lot of this comes from the [OSM wiki](https://wiki.openstreetmap.org/))

For chiasmos' purposes, OSM represents roads using individual nodes (a single point in space represented by its lat, long, and node id)
which are grouped into a chain called a way. Ways describe roads, as well as other linear features like walls or rivers. Ways store nodes
in an order, which represents the direction of travel on one-way roads defined by those nodes.

A vertex is a node that road ways reference more than once, e.g. because two roads share it (an intersection) or one road loops back on itself.

## Todo
- Investigate the PMR stack buffer size

# License
chiasmos is licensed under the GNU Affero General Public License v3.0 or later; see [LICENSE](LICENSE).
OpenStreetMap data, and indexes built from it, are licensed separately under the [ODbL](https://opendatacommons.org/licenses/odbl/).

# Acknowledgements
chiasmos is built on [OpenStreetMap](https://www.openstreetmap.org/copyright) data, © OpenStreetMap contributors.
Thanks to [Geofabrik](https://download.geofabrik.de/) for the regional extracts chiasmos is built and tested with.
