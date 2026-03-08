#!/usr/bin/env python3
"""Convert nanobench JSON output to github-action-benchmark customSmallerIsBetter format.

Reads:  bench-results/bench_canvas.json
        bench-results/bench_ecs.json
        bench-results/bench_lua.json
Writes: bench-results/bench-combined.json

Each input file has structure:
    {"results": [{"name": "...", "median(elapsed)": <seconds>, ...}, ...]}

Output format (customSmallerIsBetter):
    [{"name": "...", "unit": "ns/op", "value": <nanoseconds>, "range": "± X.XX%"}, ...]
"""
import json
import os
import sys

RESULTS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'bench-results')
INPUT_FILES = ['bench_canvas.json', 'bench_ecs.json', 'bench_lua.json']
OUTPUT_FILE = os.path.join(RESULTS_DIR, 'bench-combined.json')


def main():
    combined = []

    for fname in INPUT_FILES:
        path = os.path.join(RESULTS_DIR, fname)

        if not os.path.exists(path):
            print(f'ERROR: missing input file: {path}', file=sys.stderr)
            sys.exit(1)

        with open(path) as f:
            data = json.load(f)

        results = data.get('results', [])
        if not results:
            print(f'ERROR: {fname} has zero results', file=sys.stderr)
            sys.exit(1)

        for r in results:
            elapsed_s = r.get('median(elapsed)', 0.0)
            mape = r.get('medianAbsolutePercentError(elapsed)', 0.0)
            combined.append({
                'name': r['name'],
                'unit': 'ns/op',
                'value': round(elapsed_s * 1e9, 4),
                'range': f'\u00b1 {round(mape * 100, 2)}%',
            })

    with open(OUTPUT_FILE, 'w') as f:
        json.dump(combined, f, indent=2)

    print(f'Wrote {len(combined)} benchmarks to {OUTPUT_FILE}')


if __name__ == '__main__':
    main()
