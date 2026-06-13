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
ADDRINT mainLow = 0, mainHigh = 0;


std::map<REG, UINT64> registerReadyCycle;
std::map<REG, bool>   isLoadReg;

// ── Per-instruction log ───────────────────────────────────────────────────────
struct InstrLog {
    UINT64      cycle;
    std::string label;
    UINT64      dataStall;
    UINT64      ctrlStall;
    bool        loadUse;
    bool        isBranch;
    bool        isMemory;
};
std::vector<InstrLog> instrLog;
static const UINT64 MAX_LOG = 1000;

VOID Image(IMG img, VOID *v)
{
    if (IMG_IsMainExecutable(img))
    {
        mainLow = IMG_LowAddress(img);
        mainHigh = IMG_HighAddress(img);
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────
static std::string escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else                out += c;
    }
    return out;
}

// ── Analysis routine ──────────────────────────────────────────────────────────
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

    bool isMemory = isLoad || isStore;
    bool loadUse  = false;
    UINT64 dStall = 0;
    UINT64 cStall = 0;

    if (isMemory) memoryAccesses++;

    auto stallForReg = [&](REG r) -> UINT64 {
        if (r == REG_INVALID()) return 0;
        auto it = registerReadyCycle.find(r);
        if (it != registerReadyCycle.end() && it->second > currentCycle)
            return it->second - currentCycle;
        return 0;
    };

    UINT64 s0 = stallForReg(read0);
    UINT64 s1 = stallForReg(read1);
    dStall = (s0 > s1) ? s0 : s1;   

    if (dStall > 0) {
        auto isLoadSource = [&](REG r) -> bool {
            if (r == REG_INVALID()) return false;
            auto it = isLoadReg.find(r);
            return (it != isLoadReg.end() && it->second);
        };
        if (isLoadSource(read0) || isLoadSource(read1)) {
            loadUse = true;
            loadUseStalls++;
        }
        dataStalls   += dStall;
        currentCycle += dStall;
    }

    
    if (isBranch) {
        cStall        = 2;
        controlStalls += 2;
        currentCycle  += 2;
    }

    
    if (write0 != REG_INVALID()) {
        UINT64 latency             = isLoad ? 4 : 3;
        registerReadyCycle[write0] = currentCycle + latency;
        isLoadReg[write0]          = (bool)isLoad;
    }

    // ── 5. Log ────────────────────────────────────────────────────────────────
    if (instrLog.size() < MAX_LOG) {
        std::string* dis = reinterpret_cast<std::string*>(disasPtr);
        InstrLog entry;
        entry.cycle     = currentCycle;
        entry.label     = dis ? escape(*dis) : "???";
        entry.dataStall = dStall;
        entry.ctrlStall = cStall;
        entry.loadUse   = loadUse;
        entry.isBranch  = (bool)isBranch;
        entry.isMemory  = isMemory;
        instrLog.push_back(entry);
    }
}
}

// ── Instrumentation routine ───────────────────────────────────────────────────
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

    std::cout << "\n=== Pipeline Analysis (WITHOUT Forwarding) ===\n";
    std::cout << "Total Instructions   : " << totalInstructions << "\n";
    std::cout << "Total Cycles         : " << currentCycle      << "\n";
    std::cout << "Data Hazard Stalls   : " << dataStalls        << "\n";
    std::cout << "  Load-Use Stalls    : " << loadUseStalls     << "\n";
    std::cout << "Control Stalls       : " << controlStalls     << "\n";
    std::cout << "Memory Accesses      : " << memoryAccesses    << "\n";
    std::cout << "Total Stall Cycles   : " << totalStalls       << "\n";
    std::cout << "Estimated CPI        : " << cpi               << "\n";
    std::cout << "==============================================\n";

    std::ofstream out("without_forwarding.json");
    if (!out.is_open()) {
        std::cerr << "Could not open without_forwarding.json\n";
        return;
    }

    out << "{\n";
    out << "  \"mode\": \"without_forwarding\",\n";
    out << "  \"summary\": {\n";
    out << "    \"totalInstructions\": " << totalInstructions << ",\n";
    out << "    \"totalCycles\": "       << currentCycle      << ",\n";
    out << "    \"dataStalls\": "        << dataStalls        << ",\n";
    out << "    \"loadUseStalls\": "     << loadUseStalls     << ",\n";
    out << "    \"forwardingEvents\": 0,\n";
    out << "    \"controlStalls\": "     << controlStalls     << ",\n";
    out << "    \"memoryAccesses\": "    << memoryAccesses    << ",\n";
    out << "    \"totalStalls\": "       << totalStalls       << ",\n";
    out << "    \"cpi\": "               << cpi               << "\n";
    out << "  },\n";

    out << "  \"instructions\": [\n";
    for (size_t i = 0; i < instrLog.size(); i++) {
        const InstrLog& e = instrLog[i];
        out << "    {\n";
        out << "      \"id\": "         << i                             << ",\n";
        out << "      \"cycle\": "      << e.cycle                       << ",\n";
        out << "      \"label\": \""    << e.label                       << "\",\n";
        out << "      \"dataStall\": "  << e.dataStall                   << ",\n";
        out << "      \"ctrlStall\": "  << e.ctrlStall                   << ",\n";
        out << "      \"forwarded\": false,\n";
        out << "      \"loadUse\": "    << (e.loadUse  ? "true":"false") << ",\n";
        out << "      \"isBranch\": "   << (e.isBranch ? "true":"false") << ",\n";
        out << "      \"isMemory\": "   << (e.isMemory ? "true":"false") << "\n";
        out << "    }" << (i + 1 < instrLog.size() ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    out.close();
    std::cout << "JSON written to: without_forwarding.json\n";
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
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