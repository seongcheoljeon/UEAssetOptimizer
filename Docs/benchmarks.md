# Benchmarks

> Placeholder for Sprint 5 performance measurements.
> Populate with `stat unit`, `stat rhi`, draw-call, and VRAM numbers once
> the plugin is fully functional.

## Methodology (planned)

1. **Test fixtures**
   - Quixel Megascans rock (source: ~180 k tri, photogrammetry-dirty)
   - Custom sculpted statue (~250 k tri, clean)
   - CAD machine part (~80 k tri, hard-edged)

2. **Test scene**
   - 1000 instances on a grid (ISM actor)
   - Third-person camera orbit path, 30 s loop
   - Direct lighting only (no Lumen) to isolate geometry cost

3. **Metrics**
   - `stat unit`   → Frame / Game / Draw / GPU (ms)
   - `stat rhi`    → Draw calls, primitives, GPU memory
   - Task Manager → CPU / VRAM peaks

4. **Variants**
   - Baseline (LOD0 only)
   - Plugin-generated LODs (4 levels, default params)
   - Plugin LODs + Alpha-Wrap-simplified source

## Expected results

| Metric | Baseline | w/ LODs | Δ |
|--------|----------|---------|---|
| GPU frame (ms) | TBD | TBD | ≥ 40 % ↓ |
| Draw calls | TBD | TBD | similar |
| Triangle count | TBD | TBD | ≥ 70 % ↓ |
| VRAM (MB) | TBD | TBD | ≥ 50 % ↓ |

Screenshots and JSON logs committed under `Docs/benchmark_data/` once captured.
