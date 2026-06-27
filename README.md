# Pipeline Stall Analyzer and Visualizer

A dynamic pipeline analysis tool built using Intel PIN to simulate a 5-stage processor pipeline and analyze hazards, stalls, and forwarding behavior. The tool records instruction-level execution details and generates comprehensive pipeline statistics for performance analysis.


---

## FEATURES

• Data hazard detection# Pipeline Stall Analyzer

A **Intel PIN-based dynamic binary instrumentation tool** that simulates a 5-stage pipeline, detects RAW data hazards, models all forwarding paths, and visualizes results in an interactive browser dashboard.

Built to compare pipeline execution **with full forwarding** vs **without forwarding** on any x86-64 binary — no source code needed.

---

## What It Does

Runs your binary under Intel PIN and tracks every instruction in real time:

- Detects **RAW (Read-After-Write) data hazards** between instructions
- Simulates all four **forwarding paths** (EX/EX, MEM/EX, EX/MEM, MEM/MEM)
- Identifies **load-use hazards** (always 1 stall cycle, even with forwarding)
- Counts **control hazard stalls** from branches (2 cycles each)
- Computes **CPI** (Cycles Per Instruction) for both modes
- Logs per-instruction stall and forwarding data (first 500 instructions)
- Outputs JSON for the visualizer

---

## Pipeline Model

```
IF → ID → EX → MEM → WB
```

| Event | Cycle |
|---|---|
| Producer issued at cycle C | C |
| EX/MEM latch written (ALU result) | C+1 |
| MEM/WB latch written (load data ready) | C+2 |
| WB completes (no-forwarding baseline) | C+3 |

### Forwarding Paths

| Path | Source Latch | Destination | Condition |
|---|---|---|---|
| **EX/EX** | EX/MEM | ALU input | Producer 1 cycle ago, ALU only |
| **MEM/EX** | MEM/WB | ALU input | Producer 2+ cycles ago |
| **EX/MEM** | EX/MEM | Store data port | Producer 1 cycle ago, ALU only |
| **MEM/MEM** | MEM/WB | Store data port | Producer 2+ cycles ago |

> **Load-use hazard**: When a load is immediately followed by a dependent instruction, the EX/MEM latch only holds the memory *address* — not the data. The data arrives at the MEM/WB latch one cycle later, causing an unavoidable **1-cycle stall** even with full forwarding.

### Without Forwarding

Without forwarding, the consumer must wait until **WB completes**:
- ALU producer → **2–3 stall cycles**
- Load producer → **3–4 stall cycles**

---

## Project Structure

```
.
├── pipeline_stall.cpp        # PIN tool: WITH full forwarding
├── without_forwarding.cpp    # PIN tool: WITHOUT forwarding
├── visualizer.html           # Interactive browser dashboard
├── serve.py                  # Local server with auto-reload
└── README.md
```

---

## Requirements

- **Intel PIN** (tested with PIN 3.x) — [download here](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-dynamic-binary-instrumentation-tool.html)
- **g++** with C++11 support
- **Python 3** (for the server)
- Any x86-64 Linux binary to analyze

---

## Build

Set your PIN root:

```bash
export PIN_ROOT=~/pin   # adjust to your PIN installation path
```

Build both tools:

```bash
# With forwarding
g++ -std=c++11 -Wall -fPIC -shared \
    -I$PIN_ROOT/source/include/pin \
    -I$PIN_ROOT/source/include/pin/gen \
    -I$PIN_ROOT/extras/components/include \
    -I$PIN_ROOT/extras/xed-intel64/include \
    -o obj-intel64/pipeline_stall.so \
    pipeline_stall.cpp

# Without forwarding
g++ -std=c++11 -Wall -fPIC -shared \
    -I$PIN_ROOT/source/include/pin \
    -I$PIN_ROOT/source/include/pin/gen \
    -I$PIN_ROOT/extras/components/include \
    -I$PIN_ROOT/extras/xed-intel64/include \
    -o obj-intel64/without_forwarding.so \
    without_forwarding.cpp
```

Or use your PIN `makefile.rules` if you have a standard PIN build setup:

```bash
make obj-intel64/pipeline_stall.so TARGET=intel64
make obj-intel64/without_forwarding.so TARGET=intel64
```

---

## Usage

### 1. Run both PIN tools on your binary

```bash
# With forwarding
~/pin/pin -t obj-intel64/pipeline_stall.so -- ./your_binary [args]

# Without forwarding
~/pin/pin -t obj-intel64/without_forwarding.so -- ./your_binary [args]
```

This produces:
- `results.json` — with-forwarding analysis
- `without_forwarding.json` — no-forwarding baseline

### 2. Start the visualizer server

```bash
python3 serve.py
```

The server opens your browser automatically at `http://localhost:8765`. It watches both JSON files and **auto-reloads** the dashboard whenever you re-run the PIN tool — no manual refresh needed.

```
Options:
  --port PORT    Port to serve on (default: 8765)
  --dir  DIR     Directory containing the JSON files (default: current dir)
```

### 3. Explore the dashboard

The dashboard has four tabs:

| Tab | What you see |
|---|---|
| **Comparison** | CPI, stall counts, speedup, cycles saved side-by-side |
| **Without Forwarding** | Latency diagrams, stall cost breakdown, pipeline timing |
| **Forwarding Paths** | Animated SVG pipeline showing each forwarding path |
| **Instruction Log** | Per-instruction table with stall counts, forwarding path fired, flags |

You can also **upload JSON files directly** from the browser without running the server.

---

## Dashboard Features

### Comparison Tab
- CPI and total cycle comparison (with vs without forwarding)
- Speedup multiplier and cycles saved
- Forwarding path breakdown (EX/EX, MEM/EX, MEM/MEM, EX/MEM, branch)
- Stall breakdown and execution profile charts

### Forwarding Paths Tab
- Interactive animated SVG pipeline diagram
- Select any forwarding path to see exactly which latch is the source and which stage receives the forwarded value
- Branch forwarding deep-dive: explains why the 2-cycle control stall is unavoidable even when data forwarding succeeds

### Instruction Log Tab
- Every instruction (up to 500) with its cycle, data stall, ctrl stall, and exact forwarding path
- **Filter by**: forwarded, data stall, ctrl stall, load-use, memory
- **Click any forwarded row** to jump to the Forwarding Paths tab with that path animated
- Toggle between with-forwarding and without-forwarding logs

---

## Example Output

```
=== Pipeline Analysis (WITH Full Forwarding) ===
Total Instructions   : 169787
Total Cycles         : 235340
Data Hazard Stalls   : 0
  Load-Use Stalls    : 751
Control Stalls       : 65598
Memory Accesses      : 79648
Total Stall Cycles   : 65598
Estimated CPI        : 1.38646

--- Forwarding Path Breakdown ---
EX/MEM → ALU  (EX/EX   path): 89450
MEM/WB → ALU  (MEM/EX  path): 14280
MEM/WB → MEM  (MEM/MEM path): 3615
EX/MEM → MEM  (EX/MEM  path): 17903
Branch forwarding events     : 15104
Total forwarding events      : 140352
```

```
=== Pipeline Analysis (WITHOUT Forwarding) ===
Total Instructions   : 169787
Total Cycles         : 389316
Data Hazard Stalls   : 153976
  Load-Use Stalls    : 1064
Control Stalls       : 65598
Total Stall Cycles   : 219574
Estimated CPI        : 2.29357
```

**Forwarding reduced CPI by ~0.91 — a 39% runtime improvement on this workload.**

---

## How the Forwarding Detection Works

Each instruction goes through 5 steps:

1. **RAW check** — `resolveFwd()` searches an in-flight circular buffer (last 4 register-writing instructions) newest-first. Checks MEM/WB latch first, then EX/MEM latch (order matters — MEM condition is a subset of EX condition so EX must be checked second to avoid shadowing MEM/EX events).

2. **Load-use classification** — if a stall remains after forwarding and the source was a load, it's a load-use stall (exactly 1 cycle).

3. **Branch handling** — branch operands are resolved by the same `resolveFwd()` call as any other instruction. A branch-forwarding tag is applied if any forwarding path fired while the instruction is a branch. The 2-cycle control stall is always added regardless.

4. **In-flight buffer update** — the result register is written into the circular buffer with `exDoneCycle = currentCycle + 1` and `memDoneCycle = currentCycle + 2`.

5. **Log** — per-instruction record saved with `fwdPath` bitmask (bit0=EX/EX, bit1=MEM/EX, bit2=MEM/MEM, bit3=EX/MEM, bit4=branch).

---

## CPI Formula

```
CPI = (totalInstructions + dataStalls + controlStalls) / totalInstructions
```

---

## Limitations

- Models a **simple in-order 5-stage pipeline** — no branch prediction, no out-of-order execution, no cache modeling
- Only tracks the **first read register and first write register** per instruction (PIN `INS_RegR(ins, 0/1)` and `INS_RegW(ins, 0)`)
- Instruction log capped at **500 entries** (configurable via `MAX_LOG` in the source)
- Stall model is a **conservative upper bound** — a real processor may reorder or overlap operations not captured here

---

## License

MIT
• Load-use hazard analysis
• Control hazard analysis
• Forwarding path detection and counting
• Per-instruction pipeline logging
• Memory access tracking
• Branch instruction analysis
• JSON-based output generation

---

## FORWARDING PATHS SUPPORTED

1. EX/MEM → EX forwarding
2. MEM/WB → EX forwarding
3. EX/MEM → MEM forwarding
4. MEM/WB → MEM forwarding
5. Branch comparator forwarding

---

## GENERATED STATISTICS

• Total instructions executed
• Total cycles
• Memory instructions
• Branch instructions
• Data stalls
• Control stalls
• Load-use hazards
• Forwarding counts
• Per-instruction execution details

---

## PROJECT STRUCTURE

pipeline_stall.cpp
Main pipeline simulator with forwarding support.

without_forwarding.cpp
Pipeline simulator without forwarding for comparison.

visualizer.html
Interactive web-based visualization interface.

serve.py
Local server for launching the visualization.

results.json
Output file containing execution statistics and logs.

---

## TECHNOLOGIES USED

• C++
• Intel PIN Dynamic Binary Instrumentation Framework
• HTML
• CSS
• JavaScript
• JSON

---

## USE CASES

• Computer Architecture projects
• Pipeline hazard analysis
• Performance evaluation
• Forwarding mechanism studies

---

## OVERVIEW

This project dynamically instruments program execution using Intel PIN and simulates a 5-stage processor pipeline. It identifies hazards, tracks forwarding events, computes stall cycles, and generates detailed execution statistics. The generated JSON output can be explored through an interactive browser-based visualizer to better understand pipeline behavior and performance.
