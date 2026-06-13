# Pipeline Stall Analyzer and Visualizer

A dynamic pipeline analysis tool built using Intel PIN to simulate a 5-stage processor pipeline and analyze hazards, stalls, and forwarding behavior. The tool records instruction-level execution details and generates comprehensive pipeline statistics for performance analysis.


---

## FEATURES

• Data hazard detection
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
