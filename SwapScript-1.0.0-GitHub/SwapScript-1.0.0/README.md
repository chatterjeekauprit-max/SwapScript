# SwapScript

SwapScript is a small declarative rule compiler and heap-free C99 runtime for selecting among parameter blocks already resident in memory. Developers write transition rules in `.swp`; the offline Python compiler hashes names and emits static C rule tables. The engine evaluates telemetry and switches the active parameter pointer after a condition remains true for its hold time.

## Features

- Human-readable `.swp` contexts and directional transition rules.
- Offline compiler using only the Python standard library.
- FNV-1a context and metric keys, with collision checks.
- Single-header C99 runtime; no heap allocation or model reload.
- Fixed-capacity context registry and bounded event metric count.
- Hold-time dwell guards, directional hysteresis, and wrap-safe millisecond ticks.
- Acquire/release pointer access with GCC/Clang builtins; override hooks for target atomics.
- CMake host demo and runtime tests.

## Where to use it

Use it when an embedded application already keeps several parameter blocks in stable memory and needs to select one based on sensor metrics. Suitable examples include PID gain profiles, filter coefficients, inference thresholds, calibration sets, and operating modes.

The engine selects parameter blocks; it does not load model weights, perform inference, read sensors, or drive actuators. It is not a safety certification or a replacement for system-level hazard analysis. The claimed sub-microsecond performance must be measured on the actual board, compiler, and workload.

## Quick start

Requirements: Python 3.8+ to compile `.swp`; C99 compiler and CMake 3.16+ to build the demo/tests.

1. Edit `demo.swp`:

   ```text
   context daylight params HEAD_DAYLIGHT
   context night params HEAD_NIGHT
   initial daylight
   transition daylight -> night when ambient_lux <= 20 hold 50 ms
   transition night -> daylight when ambient_lux >= 30 hold 100 ms
   ```

2. Generate the rule table:

   ```sh
   python3 compiler.py demo.swp -o swapscript_generated.h
   ```

The compiler defaults to 16 contexts and 32 distinct metric names, matching the engine defaults. If you change `SS_MAX_CONTEXTS` or `SS_MAX_METRICS`, pass the same capacities with `--max-contexts` and `--max-metrics` when compiling the DSL.

3. In C, define your static parameter blocks, include `swapscript.h` and the generated header, register contexts, load rules, then set the initial context. `demo.c` shows the full setup.

4. Before the existing pipeline runs, submit matching metric keys and values:

   ```c
   uint32_t keys[] = { SS_METRIC_AMBIENT_LUX };
   float values[] = { current_lux };
   const ai_head_params_t *active = (const ai_head_params_t *)
       ss_process_event(&engine, keys, values, 1u, now_ms);
   run_inference(frame, active->conf_thresh, active->nms_iou);
   ```

Parameter blocks and the generated rule table are referenced, not copied. Keep them alive and immutable while they may be read. One task must own `ss_process_event`; concurrent readers can use `ss_active_parameters` if the configured pointer operations are truly atomic on the target.

## Build and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Run compiler unit tests separately:

```sh
python3 -m unittest discover -s tests -v
```

## Runtime limits and production notes

- Set `SS_MAX_CONTEXTS` and `SS_MAX_METRICS` at compile time for the product's fixed resource budget.
- Rule evaluation is O(rule count × metric count); pointer publication is constant-time. Keep the DSL small or index metrics/rules for tighter execution budgets.
- `hold` values must be less than 2^31 milliseconds. Timer subtraction is unsigned and wrap-safe in this range.
- GCC/Clang use acquire/release `__atomic` builtins. ISO C99 itself has no atomic pointers; other compilers default to plain pointer access, suitable only with serialized access. For concurrent readers, define both `SS_POINTER_LOAD(p)` and `SS_POINTER_STORE(p, v)` to supported platform primitives.
- Confirm pointer alignment, lock-free behavior, timer source, execution time, and interrupt/task interactions on the target MCU before using hard real-time claims.

## Project files

- `compiler.py` — `.swp` parser and C header generator.
- `swapscript.h` — single-header C99 runtime.
- `demo.swp`, `demo.c` — end-to-end sample.
- `tests/` — compiler unit tests and C runtime tests.
- `CMakeLists.txt` — host demo and CTest configuration.

License: MIT. See `LICENSE`.
