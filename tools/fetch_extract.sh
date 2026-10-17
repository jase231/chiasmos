#!/bin/sh
# Download a Geofabrik state extract and verify its MD5:  
# tools/fetch_extract.sh virginia [dest-dir]
# The extract lands in data/osm/<name>-latest.osm.pbf
set -eu
name=${1:?usage: fetch_extract.sh <geofabrik-name, e.g. virginia> [dest-dir]}
dest=${2:-data/osm}
case "$name" in
  */*) url="https://download.geofabrik.de/$name-latest.osm.pbf" ;;
  *)   url="https://download.geofabrik.de/north-america/us/$name-latest.osm.pbf" ;;
esac
mkdir -p "$dest"
out="$dest/$(basename "$name")-latest.osm.pbf"
curl -fL --retry 3 -o "$out.part" "$url"
want=$(curl -fsL "$url.md5" | cut -d' ' -f1)
got=$(md5sum "$out.part" | cut -d' ' -f1)
[ "$want" = "$got" ] || { echo "MD5 mismatch for $url" >&2; rm -f "$out.part"; exit 1; }
mv "$out.part" "$out"
echo "$out"
