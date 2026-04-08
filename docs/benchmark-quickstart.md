# Benchmark Quickstart

This is the shortest path to a paper-grade benchmark run from a fresh clone.

If you only want to confirm that the binary runs, use the legacy smoke commands
in [`README.md`](/Users/macbook/Desktop/workspace/SOPMOA/README.md). For
benchmark results intended for comparison, use the workflow below.

## 1. Build

macOS:

```bash
brew install cmake boost tbb
cmake -S . -B Release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix boost);$(brew --prefix tbb)"
cmake --build Release -j4
```

Linux:

```bash
sudo apt update
sudo apt install -y build-essential cmake libboost-program-options-dev libtbb-dev
cmake -S . -B Release -DCMAKE_BUILD_TYPE=Release
cmake --build Release -j4
```

## 2. Run Raw Benchmark Suites

Example suite set:

```bash
python3 bench/scripts/run_benchmark.py \
  --config bench/configs/completion.yaml \
  --suite-id paper_completion_demo

python3 bench/scripts/run_benchmark.py \
  --config bench/configs/timecap.yaml \
  --suite-id paper_timecap_demo

python3 bench/scripts/run_benchmark.py \
  --config bench/configs/scaling.yaml \
  --suite-id paper_scaling_demo
```

Raw outputs go to:

- `bench/results/paper_completion_demo/`
- `bench/results/paper_timecap_demo/`
- `bench/results/paper_scaling_demo/`

## 3. Aggregate

```bash
python3 bench/scripts/aggregate_results.py \
  --suite paper_completion_demo \
  --suite paper_timecap_demo \
  --suite paper_scaling_demo \
  --analysis-id paper_demo
```

Aggregate outputs go to:

- `bench/results/figures/paper_demo/`

## 4. Render Figures

```bash
python3 bench/scripts/plot_results.py \
  --input-dir bench/results/figures/paper_demo
```

Figures go to:

- `bench/results/figures/paper_demo/figures/`

## 5. Read The Results

Start with:

- `bench/results/figures/paper_demo/completion_summary.csv`
- `bench/results/figures/paper_demo/scaling_summary.csv`
- `bench/results/figures/paper_demo/anytime_summary.csv`
- `bench/results/figures/paper_demo/aggregate_manifest.json`

The aggregate manifest records:

- source suites
- formulas
- current metric limits
- output directory

## Which Docs To Read Next

- Protocol: [benchmark-protocol.md](/Users/macbook/Desktop/workspace/SOPMOA/docs/benchmark-protocol.md)
- Metrics: [metrics-definition.md](/Users/macbook/Desktop/workspace/SOPMOA/docs/metrics-definition.md)
- Reproducibility: [reproducibility.md](/Users/macbook/Desktop/workspace/SOPMOA/docs/reproducibility.md)
