================================================================
  PIPELINE STALL ANALYZER — Setup & Usage Guide
================================================================

FILES IN THIS PROJECT:
  pipeline_stall.cpp   →  PIN tool source code
  makefile.rules       →  Build rules for PIN
  visualizer.html      →  Browser-based UI (open this in Chrome/Firefox)

----------------------------------------------------------------
STEP 1 — Copy files to PIN tools directory
----------------------------------------------------------------

cp pipeline_stall.cpp  ~/pin/source/tools/PipelineStall/
cp makefile.rules       ~/pin/source/tools/PipelineStall/

(Makefile should already be there from your earlier setup)

----------------------------------------------------------------
STEP 2 — Build the tool
----------------------------------------------------------------

cd ~/pin/source/tools/PipelineStall
make

----------------------------------------------------------------
STEP 3 — Run on your program
----------------------------------------------------------------

~/pin/pin -t obj-intel64/pipeline_stall.so -o results.json -- ./your_program

Example:
  ~/pin/pin -t obj-intel64/pipeline_stall.so -o results.json -- ./test

This will:
  - Print results in terminal (as before)
  - Also write a results.json file in current directory

----------------------------------------------------------------
STEP 4 — Visualize
----------------------------------------------------------------

1. Open visualizer.html in your browser (double click it)
2. Click "Load results.json"
3. Select the results.json file generated in Step 3
4. See full pipeline analysis with:
   - Stall breakdown charts
   - Per-instruction pipeline stage view
   - Forwarding events
   - Load-use hazards
   - Instruction-level log with filters

----------------------------------------------------------------
WHAT'S NEW IN THE CODE (vs original):
----------------------------------------------------------------

Feature                   Description
---------                 -----------
Forwarding model          If stall <= 2 cycles, forwarding covers it (no stall)
Load-use hazard           Separate detection when load result used immediately
Memory access tracking    Counts all load/store instructions
INS_Disassemble()         Actual instruction text in JSON (e.g. "add eax, ebx")
KnobOutputFile            -o flag to set output filename
JSON output               Full results.json with summary + per-instruction data
Visualizer UI             Browser-based visualization of all results

----------------------------------------------------------------
SAMPLE OUTPUT (terminal):
----------------------------------------------------------------

=== Pipeline Stall Analysis ===
Total Instructions   : 122563
Total Cycles         : 251592
Data Hazard Stalls   : 62421
  Load-Use Stalls    : 8200
  Forwarding events  : 20000
Control Stalls       : 46608
Memory Accesses      : 31200
Total Stall Cycles   : 129029
Estimated CPI        : 2.052
================================
JSON written to: results.json

================================================================
