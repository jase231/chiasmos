#!/usr/bin/env python3
"""Score geocoder batch output against truth points.

  tools/accuracy.py out.csv --q-col "Block/Intersection" --truth truth_ng911.csv [--truth truth_tiger.csv]
                    [--negatives negatives-out.csv] [--radius 50 100]
                    [--diff other.csv --diff-out diff.tsv] [--json report.json]

Results: the CSV written by `chiasmos batch` (columns chiasmos_status/lat/lon/score/kind/alternatives). Truth: one row per distinct
location string, columns q,n_points,points where points are "lat lon;lat lon". A row counts as correct when its top answer is within
the radius of the NEAREST truth point.
"""
import argparse
import csv
import json
import math
import sys

FOUND = {"ok", "ambiguous"}                        # the geocoder gave an answer
STREETS_FOUND = FOUND | {"no_intersection"}        # ...or at least both streets exist in the index
PREFIX = "chiasmos_"                               # column prefix of `chiasmos batch` output


def haversine_m(a, b):
    (la1, lo1), (la2, lo2) = a, b
    p1, p2 = math.radians(la1), math.radians(la2)
    h = math.sin((p2 - p1) / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(math.radians(lo2 - lo1) / 2) ** 2
    return 2 * 6371008.8 * math.asin(math.sqrt(h))


def load_truth(path):
    truth = {}
    with open(path, newline="", encoding="utf-8-sig") as f:
        for row in csv.DictReader(f):
            pts = [tuple(map(float, p.split())) for p in row["points"].split(";") if p.strip()]
            if pts:
                truth[row["q"]] = pts
    return truth


def load_results(path, q_col):
    with open(path, newline="", encoding="utf-8-sig") as f:
        rows = list(csv.DictReader(f))
    out = []
    for r in rows:
        point = None
        if r.get(PREFIX + "lat") and r.get(PREFIX + "lon"):
            point = (float(r[PREFIX + "lat"]), float(r[PREFIX + "lon"]))
        alts = []
        for item in filter(None, (r.get(PREFIX + "alternatives") or "").split(";")):
            parts = item.split()
            alts.append((float(parts[0]), float(parts[1])))
        out.append({"q": r[q_col], "status": r[PREFIX + "status"], "point": point, "alts": alts,
                    "kind": r.get(PREFIX + "kind", ""), "score": float(r.get(PREFIX + "score") or 0)})
    return out


def percentile(sorted_vals, p):  # nearest rank: every reported value is an error that occurred
    return sorted_vals[round(p * (len(sorted_vals) - 1))] if sorted_vals else float("nan")


def score(rows, truth, radii):
    with_truth = [r for r in rows if r["q"] in truth]
    found = [r for r in with_truth if r["status"] in FOUND and r["point"]]
    streets = [r for r in with_truth if r["status"] in STREETS_FOUND]
    err = lambda r: min(haversine_m(r["point"], t) for t in truth[r["q"]])  # noqa: E731
    errors = sorted(err(r) for r in found)
    report = {"rows": len(rows), "rows_with_truth": len(with_truth), "answered": len(found), "streets_found": len(streets),
              "error_m": {f"p{int(p * 100)}": percentile(errors, p) for p in (0.5, 0.9, 0.95, 0.99, 1.0)}}
    for R in radii:
        hit = sum(1 for r in found if err(r) <= R)
        near_alt = sum(1 for r in with_truth if r["point"] and any(
            min(haversine_m(p, t) for t in truth[r["q"]]) <= R for p in [r["point"], *r["alts"]]))
        report[f"within_{R:g}m"] = {"of_rows_with_truth": hit / max(1, len(with_truth)), "of_streets_found": hit / max(1, len(streets)),
                                   "recall_at_k": near_alt / max(1, len(with_truth))}
    kinds = {}
    for r in found:
        k = kinds.setdefault(r["kind"], [0, 0])
        k[0] += 1
        k[1] += err(r) <= radii[0]
    report["by_kind"] = {k: {"n": n, f"within_{radii[0]:g}m": h / n} for k, (n, h) in sorted(kinds.items())}
    return report, {id(r): err(r) for r in found}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("results")
    ap.add_argument("--q-col", default="Block/Intersection")
    ap.add_argument("--truth", action="append", required=True)
    ap.add_argument("--negatives", help="batch output for street pairs that do NOT meet (column q): reports the false-positive rate")
    ap.add_argument("--radius", type=float, nargs="+", default=[50, 100])
    ap.add_argument("--diff", help="second results file (e.g. another build): list rows where exactly one of the two is within the first radius")
    ap.add_argument("--diff-out", default="diff.tsv")
    ap.add_argument("--json")
    a = ap.parse_args()

    rows = load_results(a.results, a.q_col)
    full = {}
    for tpath in a.truth:
        truth = load_truth(tpath)
        rep, errs = score(rows, truth, a.radius)
        full[tpath] = rep
        print(f"\n== {a.results} vs {tpath}: {rep['rows_with_truth']} of {rep['rows']} rows have truth")
        print(f"   answered {rep['answered']}, both streets found {rep['streets_found']}")
        for R in a.radius:
            w = rep[f"within_{R:g}m"]
            print(f"   within {R:g} m: {100 * w['of_rows_with_truth']:.1f}% of rows with truth, {100 * w['of_streets_found']:.1f}% of rows "
                  f"whose streets were found, recall@k {100 * w['recall_at_k']:.1f}%")
        print("   error m: " + ", ".join(f"{k} {v:.1f}" for k, v in rep["error_m"].items()))
        if a.diff and tpath == a.truth[0]:
            other = load_results(a.diff, a.q_col)
            _, other_errs = score(other, truth, a.radius)
            seen, n = set(), 0
            with open(a.diff_out, "w", encoding="utf-8") as out:
                out.write("q\tthis_status\tthis_err_m\tother_status\tother_err_m\n")
                for r, o in zip(rows, other):
                    this_ok = id(r) in errs and errs[id(r)] <= a.radius[0]
                    other_ok = id(o) in other_errs and other_errs[id(o)] <= a.radius[0]
                    if r["q"] in truth and this_ok != other_ok and r["q"] not in seen:
                        seen.add(r["q"])
                        n += 1
                        out.write(f"{r['q']}\t{r['status']}\t{errs.get(id(r), float('nan')):.1f}\t{o['status']}\t{other_errs.get(id(o), float('nan')):.1f}\n")
            print(f"   {n} distinct strings differ from {a.diff} at {a.radius[0]:g} m -> {a.diff_out}")

    if a.negatives:
        neg = load_results(a.negatives, "q")
        for floor in (0.0, 0.5, 0.8):
            fp = sum(1 for r in neg if r["status"] in FOUND and r["score"] >= floor)
            print(f"\nfalse positives (score >= {floor}): {fp} of {len(neg)} pairs that share no node")
            full.setdefault("negatives", {})[f"score_ge_{floor}"] = fp / max(1, len(neg))
    if a.json:
        with open(a.json, "w") as f:
            json.dump(full, f, indent=2)


if __name__ == "__main__":
    sys.exit(main())
