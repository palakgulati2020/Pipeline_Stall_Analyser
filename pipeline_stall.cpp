#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>

// ── Counters ──────────────────────────────────────────────────────────────────
UINT64 totalInstructions = 0;
UINT64 dataStalls        = 0;
UINT64 loadUseStalls     = 0;
UINT64 controlStalls     = 0;
UINT64 memoryAccesses    = 0;
UINT64 currentCycle      = 0;

// Forwarding path counters
UINT64 fwd_EX_EX       = 0;   // EX/MEM → ALU input   (ALU producer, 1 cycle ago)
UINT64 fwd_MEM_EX      = 0;   // MEM/WB → ALU input   (any producer, 2+ cycles ago)
UINT64 fwd_EX_MEM      = 0;   // EX/MEM → store data  (ALU producer, 1 cycle ago)
UINT64 fwd_MEM_MEM     = 0;   // MEM/WB → store data  (any producer, 2+ cycles ago)
UINT64 fwd_branch      = 0;   // forwarding to branch comparator
UINT64 totalForwarding = 0;
ADDRINT mainLow = 0, mainHigh = 0;

VOID Image(IMG img, VOID *v)
{
    if (IMG_IsMainExecutable(img))
    {
        mainLow = IMG_LowAddress(img);
        mainHigh = IMG_HighAddress(img);
    }
}


struct InFlight {
    REG    wReg;
    bool   isLoad;
    bool   valid;
    UINT64 exDoneCycle;    // end of EX stage (EX/MEM latch written)
    UINT64 memDoneCycle;   // end of MEM stage (MEM/WB latch written)
};

static const int INFLIGHT_SZ = 4;
InFlight inflight[INFLIGHT_SZ];
int inflightHead = 0;

// ── Per-instruction log ───────────────────────────────────────────────────────
struct InstrLog {
    UINT64      cycle;
    std::string label;
    UINT64      dataStall;
    UINT64      ctrlStall;
    bool        forwarded;
    bool        loadUse;
    bool        isBranch;
    bool        isMemory;
    bool        isStore;
    int         fwdPath;   // bitmask: bit0=EX/EX, bit1=MEM/EX, bit2=MEM/MEM,
                           //          bit3=EX/MEM, bit4=branch
};
std::vector<InstrLog> instrLog;
static const UINT64 MAX_LOG = 1000;

static std::string escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else                out += c;
    }
    return out;
}

int resolveFwd(REG srcReg, bool isStore, int& fwdPath)
{
    if (srcReg == REG_INVALID()) return 0;

    for (int i = 0; i < INFLIGHT_SZ; i++) {
        int idx = ((inflightHead - 1 - i) + INFLIGHT_SZ) % INFLIGHT_SZ;
        const InFlight& inf = inflight[idx];
        if (!inf.valid || inf.wReg != srcReg) continue;

        // ── 1. MEM/WB latch (check FIRST — covers ALU and load producers) ────
        if (inf.memDoneCycle <= currentCycle) {
            if (!isStore) {
                fwdPath |= 0x02;
                fwd_MEM_EX++;
            } else {
                fwdPath |= 0x04;
                fwd_MEM_MEM++;
            }
            totalForwarding++;
            return 0;
        }

        // ── 2. EX/MEM latch (ALU producers only — load data not here yet) ────
        if (inf.exDoneCycle <= currentCycle && !inf.isLoad) {
            if (!isStore) {
                fwdPath |= 0x01;
                fwd_EX_EX++;
            } else {
                fwdPath |= 0x08;
                fwd_EX_MEM++;
            }
            totalForwarding++;
            return 0;
        }

        return (int)(inf.memDoneCycle - currentCycle); // very very imp brother
    }

    return 0;
}


// ── Main analysis routine ─────────────────────────────────────────────────────
VOID AnalyzeInstruction(VOID* ip,
                        UINT32 r0, UINT32 r1, UINT32 w0,
                        BOOL isBranch, BOOL isLoad, BOOL isStore,
                        VOID* disasPtr)
{
    ADDRINT ipadd = reinterpret_cast<ADDRINT>(ip);
    if (ipadd >= mainLow && ipadd <= mainHigh){
    totalInstructions++;
    currentCycle++;

    REG read0  = (REG)r0;
    REG read1  = (REG)r1;
    REG write0 = (REG)w0;

    bool isMemory  = isLoad || isStore;
    bool forwarded = false;
    bool loadUse   = false;
    UINT64 dStall  = 0;
    UINT64 cStall  = 0;
    int fwdPath    = 0;

    if (isMemory) memoryAccesses++;

    // ── 1. RAW data hazard: resolve via forwarding ────────────────────────────
    int fwdPath0 = 0, fwdPath1 = 0;
    int stall0 = resolveFwd(read0, (bool)isStore, fwdPath0);
    int stall1 = resolveFwd(read1, (bool)isStore, fwdPath1);

    if (fwdPath0 || fwdPath1) {
        forwarded = true;
        fwdPath   = fwdPath0 | fwdPath1;
    }

    dStall = (UINT64)((stall0 > stall1) ? stall0 : stall1);

    // ── 2. Load-use classification ────────────────────────────────────────────
    // Any residual stall whose source is a load = load-use hazard (always 1 cycle)
    if (dStall > 0) {
        auto isLoadSource = [&](REG r) -> bool {
            if (r == REG_INVALID()) return false;
            for (int i = 0; i < INFLIGHT_SZ; i++) {
                int idx = ((inflightHead - 1 - i) + INFLIGHT_SZ) % INFLIGHT_SZ;
                const InFlight& inf = inflight[idx];
                if (!inf.valid || inf.wReg != r) continue;
                return inf.isLoad;
            }
            return false;
        };
        if (isLoadSource(read0) || isLoadSource(read1)) {
            loadUse = true;
            loadUseStalls++;
        }
        dataStalls   += dStall;
        currentCycle += dStall;
    }

    if (isBranch) {
       
        if (fwdPath & (0x01 | 0x02 | 0x04 | 0x08)) {
            fwdPath   |= 0x10;
            fwd_branch++;
            forwarded = true;
        }
        cStall        = 2;
        controlStalls += 2;
        currentCycle  += 2;
    }

    // ── 4. Update in-flight buffer ────────────────────────────────────────────
    if (write0 != REG_INVALID()) {
        InFlight& slot    = inflight[inflightHead];
        slot.wReg         = write0;
        slot.isLoad       = (bool)isLoad;
        slot.valid        = true;
        slot.exDoneCycle  = currentCycle + 1;   // EX/MEM latch: ready next cycle
        slot.memDoneCycle = currentCycle + 2;   // MEM/WB latch: ready 2 cycles later
        inflightHead = (inflightHead + 1) % INFLIGHT_SZ;
    }

    // ── 5. Log ────────────────────────────────────────────────────────────────
    if (instrLog.size() < MAX_LOG) {
        std::string* dis = reinterpret_cast<std::string*>(disasPtr);
        InstrLog entry;
        entry.cycle     = currentCycle;
        entry.label     = dis ? escape(*dis) : "???";
        entry.dataStall = dStall;
        entry.ctrlStall = cStall;
        entry.forwarded = forwarded;
        entry.loadUse   = loadUse;
        entry.isBranch  = (bool)isBranch;
        entry.isMemory  = isMemory;
        entry.isStore   = (bool)isStore;
        entry.fwdPath   = fwdPath;
        instrLog.push_back(entry);
    }
    }
}

// ── Instrumentation ───────────────────────────────────────────────────────────
VOID Instruction(INS ins, VOID* v)
{
    REG r0 = (INS_MaxNumRRegs(ins) > 0) ? INS_RegR(ins, 0) : REG_INVALID();
    REG r1 = (INS_MaxNumRRegs(ins) > 1) ? INS_RegR(ins, 1) : REG_INVALID();
    REG w0 = (INS_MaxNumWRegs(ins) > 0) ? INS_RegW(ins, 0) : REG_INVALID();

    bool isLoad  = INS_IsMemoryRead(ins);
    bool isStore = INS_IsMemoryWrite(ins);

    std::string* disas = new std::string(INS_Disassemble(ins));

    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AnalyzeInstruction,
                   IARG_INST_PTR,
                   IARG_UINT32, (UINT32)r0,
                   IARG_UINT32, (UINT32)r1,
                   IARG_UINT32, (UINT32)w0,
                   IARG_BOOL,   INS_IsBranch(ins),
                   IARG_BOOL,   isLoad,
                   IARG_BOOL,   isStore,
                   IARG_PTR,    disas,
                   IARG_END);
}

// ── Fini ──────────────────────────────────────────────────────────────────────
VOID Fini(INT32 code, VOID* v)
{
    UINT64 totalStalls = dataStalls + controlStalls;
    double cpi = totalInstructions > 0
        ? (double)(totalInstructions + totalStalls) / totalInstructions
        : 0.0;

    std::cout << "\n=== Pipeline Analysis (WITH Full Forwarding) ===\n";
    std::cout << "Total Instructions   : " << totalInstructions  << "\n";
    std::cout << "Total Cycles         : " << currentCycle       << "\n";
    std::cout << "Data Hazard Stalls   : " << dataStalls         << "\n";
    std::cout << "  Load-Use Stalls    : " << loadUseStalls      << "\n";
    std::cout << "Control Stalls       : " << controlStalls      << "\n";
    std::cout << "Memory Accesses      : " << memoryAccesses     << "\n";
    std::cout << "Total Stall Cycles   : " << totalStalls        << "\n";
    std::cout << "Estimated CPI        : " << cpi                << "\n";
    std::cout << "\n--- Forwarding Path Breakdown ---\n";
    std::cout << "EX/MEM → ALU  (EX/EX   path): " << fwd_EX_EX    << "\n";
    std::cout << "MEM/WB → ALU  (MEM/EX  path): " << fwd_MEM_EX   << "\n";
    std::cout << "MEM/WB → MEM  (MEM/MEM path): " << fwd_MEM_MEM  << "\n";
    std::cout << "EX/MEM → MEM  (EX/MEM  path): " << fwd_EX_MEM   << "\n";
    std::cout << "Branch forwarding events     : " << fwd_branch   << "\n";
    std::cout << "Total forwarding events      : " << totalForwarding << "\n";
    std::cout << "================================================\n";

    std::ofstream out("results.json");
    if (!out.is_open()) { std::cerr << "Could not open results.json\n"; return; }

    out << "{\n";
    out << "  \"mode\": \"with_forwarding\",\n";
    out << "  \"summary\": {\n";
    out << "    \"totalInstructions\": " << totalInstructions  << ",\n";
    out << "    \"totalCycles\": "       << currentCycle       << ",\n";
    out << "    \"dataStalls\": "        << dataStalls         << ",\n";
    out << "    \"loadUseStalls\": "     << loadUseStalls      << ",\n";
    out << "    \"forwardingEvents\": "  << totalForwarding    << ",\n";
    out << "    \"fwd_EX_EX\": "         << fwd_EX_EX          << ",\n";
    out << "    \"fwd_MEM_EX\": "        << fwd_MEM_EX         << ",\n";
    out << "    \"fwd_MEM_MEM\": "       << fwd_MEM_MEM        << ",\n";
    out << "    \"fwd_EX_MEM\": "        << fwd_EX_MEM         << ",\n";
    out << "    \"fwd_branch\": "        << fwd_branch         << ",\n";
    out << "    \"controlStalls\": "     << controlStalls      << ",\n";
    out << "    \"memoryAccesses\": "    << memoryAccesses     << ",\n";
    out << "    \"totalStalls\": "       << totalStalls        << ",\n";
    out << "    \"cpi\": "               << cpi                << "\n";
    out << "  },\n";

    out << "  \"instructions\": [\n";
    for (size_t i = 0; i < instrLog.size(); i++) {
        const InstrLog& e = instrLog[i];
        out << "    {\n";
        out << "      \"id\": "         << i                              << ",\n";
        out << "      \"cycle\": "      << e.cycle                        << ",\n";
        out << "      \"label\": \""    << e.label                        << "\",\n";
        out << "      \"dataStall\": "  << e.dataStall                    << ",\n";
        out << "      \"ctrlStall\": "  << e.ctrlStall                    << ",\n";
        out << "      \"forwarded\": "  << (e.forwarded ? "true":"false") << ",\n";
        out << "      \"loadUse\": "    << (e.loadUse   ? "true":"false") << ",\n";
        out << "      \"isBranch\": "   << (e.isBranch  ? "true":"false") << ",\n";
        out << "      \"isMemory\": "   << (e.isMemory  ? "true":"false") << ",\n";
        out << "      \"isStore\": "    << (e.isStore   ? "true":"false") << ",\n";
        out << "      \"fwdPath\": "    << e.fwdPath                      << "\n";
        out << "    }" << (i + 1 < instrLog.size() ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    out.close();
    std::cout << "JSON written to: results.json\n";
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    
    for (int i = 0; i < INFLIGHT_SZ; i++) inflight[i].valid = false;

    if (PIN_Init(argc, argv)) {
        std::cerr << "PIN_Init failed\n";
        return 1;
    }
    IMG_AddInstrumentFunction(Image, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();
    return 0;
}