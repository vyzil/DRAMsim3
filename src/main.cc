#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <functional>
#include <deque>
#include "memory_system.h"
#include "common.h"
#include "logger.h" 

using namespace dramsim3;

std::vector<Transaction> LoadTraceFile(const std::string& trace_file) {
    std::vector<Transaction> trace;
    std::ifstream fin(trace_file);
    if (!fin) {
        throw std::runtime_error("Failed to open trace file: " + trace_file);
    }

    std::string rw;
    uint64_t addr;
    while (fin >> std::hex >> addr >> rw) {
        trace.emplace_back(addr, rw == "W");
    }

    return trace;
}

// Channel Distribute
void PreprocessTrace(const std::vector<Transaction>& input_trace,
                     std::deque<Transaction> channel_queues[8],
                     const MemorySystem* mem) {
    std::deque<Transaction> pseudo_queues[16];

    // 1. 모든 트랜잭션을 pseudo channel 별로 분류
    for (const auto& t : input_trace) {
        int pseudo_ch = mem->GetDramSystem()->GetChannel(t.addr);
        assert(pseudo_ch >= 0 && pseudo_ch < 16);
        pseudo_queues[pseudo_ch].push_back(t);
    }

    for (int i = 0; i < 16; ++i) {
        auto& dq = pseudo_queues[i];

        // deque -> vector로 복사 후 정렬
        std::vector<Transaction> sorted(dq.begin(), dq.end());
        std::sort(sorted.begin(), sorted.end(), [](const Transaction& a, const Transaction& b) {
            return a.addr < b.addr;
        });

        // 정렬된 결과를 다시 deque로
        dq = std::deque<Transaction>(sorted.begin(), sorted.end());
    }

    // 2. 2개씩 묶어서 channel queue로 번갈아가며 넣기
    for (int ch = 0; ch < 8; ++ch) {
        auto& q0 = pseudo_queues[2 * ch];
        auto& q1 = pseudo_queues[2 * ch + 1];
        auto& out = channel_queues[ch];

        bool toggle = true;
        while (!q0.empty() || !q1.empty()) {
            if (toggle && !q0.empty()) {
                out.push_back(q0.front()); q0.pop_front();
            } else if (!toggle && !q1.empty()) {
                out.push_back(q1.front()); q1.pop_front();
            }
            toggle = !toggle;
        }
    }
}

// // Row Locality, Bank.Bankgroup Parallelism
// void PreprocessTrace(const std::vector<Transaction>& input_trace,
//                      std::deque<Transaction> channel_queues[8],
//                      const MemorySystem* mem) {
//     auto* config = mem->GetConfig();

//     // Step 1: 분류 hbm_map[channel][bg][ba]
//     std::map<int, std::map<int, std::map<int, std::vector<Transaction>>>> hbm_map;
//     for (const auto& t : input_trace) {
//         Address addr = config->AddressMapping(t.addr);
//         hbm_map[addr.channel][addr.bankgroup][addr.bank].push_back(t);
//     }

//     std::deque<Transaction> pseudo_queues[16];
//     // Step 2: 각 pseudo channel마다 bankgroup_queues[4] 준비
//     for (int ch = 0; ch < 16; ++ch) {
//         std::deque<Transaction> bankgroup_queues[4]; // BG 0~3

//         for (int bg = 0; bg < 4; ++bg) {
//             std::deque<Transaction> bank_queues[4]; // BA 0~3

//             for (int ba = 0; ba < 4; ++ba) {
//                 auto& txns = hbm_map[ch][bg][ba];

//                 // addr 기준 정렬 → row locality 확보
//                 std::sort(txns.begin(), txns.end(), [](const Transaction& a, const Transaction& b) {
//                     return a.addr < b.addr;
//                 });

//                 for (const auto& t : txns) {
//                     bank_queues[ba].push_back(t);
//                 }
//             }

//             // bank round-robin → 하나의 BG 큐로
//             bool progress = true;
//             while (progress) {
//                 progress = false;
//                 for (int ba = 0; ba < 4; ++ba) {
//                     if (!bank_queues[ba].empty()) {
//                         bankgroup_queues[bg].push_back(bank_queues[ba].front());
//                         bank_queues[ba].pop_front();
//                         progress = true;
//                     }
//                 }
//             }
//         }
//         // Step 3: bankgroup 0~3 순서로 round-robin → pseudo queue에 push
//         size_t index[4] = {0};
//         bool progress = true;
//         while (progress) {
//             progress = false;
//             for (int bg = 0; bg < 4; ++bg) {
//                 if (index[bg] < bankgroup_queues[bg].size()) {
//                     pseudo_queues[ch].push_back(bankgroup_queues[bg][index[bg]]);
//                     index[bg]++;
//                     progress = true;
//                 }
//             }
//         }


//     }

//     // Step 4: pseudo 2개씩 → 실제 channel 1개로 번갈아 분배
//     for (int ch = 0; ch < 8; ++ch) {
//         auto& q0 = pseudo_queues[2 * ch];
//         auto& q1 = pseudo_queues[2 * ch + 1];
//         auto& out = channel_queues[ch];

//         bool toggle = true;
//         while (!q0.empty() || !q1.empty()) {
//             if (toggle && !q0.empty()) {
//                 out.push_back(q0.front());
//                 q0.pop_front();
//             } else if (!toggle && !q1.empty()) {
//                 out.push_back(q1.front());
//                 q1.pop_front();
//             }
//             toggle = !toggle;
//         }
//     }    
// }



void RunSimulation(MemorySystem* mem,
                   std::deque<Transaction> channel_queues[8],
                   size_t total_transactions) {
    size_t completed = 0;
    uint64_t clk = 0;

    auto read_cb  = [&](uint64_t addr) { ++completed; };
    auto write_cb = [&](uint64_t addr) { ++completed; };
    mem->RegisterCallbacks(read_cb, write_cb);  // optional override

    auto* config = mem->GetConfig();

    while (completed < total_transactions) {
        Logger::PrintCycle(clk);
        mem->ClockTick();

        for (int ch = 0; ch < 8; ++ch) {
            if (!channel_queues[ch].empty()) {
                auto& t = channel_queues[ch].front();
                if (mem->WillAcceptTransaction(t.addr, t.is_write)) {                    
                    mem->AddTransaction(t.addr, t.is_write);
                    channel_queues[ch].pop_front();
                }
            }
        }
        clk++;
    }

    std::cout << "[Summary] Completed in " << clk << " cycles\n";
    // std::cout << std::flush << "        \r" << std::flush << clk << "\n";
    mem->PrintStats();
}


int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-D" && i + 1 < argc) {            
            g_print_cycle = true;
            std::string opt = argv[++i];
            if (opt == "all") {
                g_print_issue = true;
                g_print_return = true;
            } else if (opt == "issue") {
                g_print_issue = true;
            } else if (opt == "return") {
                g_print_return = true;
            } else {
                std::cerr << "Unknown debug option: " << opt << std::endl;
            }
        }
    }

    const std::string config_file = "configs/VCU118_HBM2_4Gb_x128.ini";
    const std::string trace_file = "traces/test.trace";
    const std::string output_dir = "output";

    Logger::Init(output_dir);

    auto read_cb = [&](uint64_t addr) { return; };
    auto write_cb = [&](uint64_t addr) { return; };

    std::vector<Transaction> trace = LoadTraceFile(trace_file);    
    MemorySystem* mem = new MemorySystem(config_file, output_dir, read_cb, write_cb);
    std::deque<Transaction> channel_queues[8];

    PreprocessTrace(trace, channel_queues, mem);
    RunSimulation(mem, channel_queues, trace.size());

    mem->PrintStats();
    delete mem;
    Logger::Close();
    return 0;
}
