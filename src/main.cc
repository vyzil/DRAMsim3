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

void RunSimulation(MemorySystem* mem,
                   std::deque<Transaction> channel_queues[8],
                   size_t total_transactions) {
    size_t completed = 0;
    uint64_t clk = 0;

    auto read_cb  = [&](uint64_t addr) { ++completed; };
    auto write_cb = [&](uint64_t addr) { ++completed; };
    mem->RegisterCallbacks(read_cb, write_cb);  // optional override

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

    // 1. trace 파일 읽기


    // // 4. 시뮬레이션 루프 (트랜잭션 모두 완료되면 종료)
    // size_t trace_index = 0;
    // size_t completed = 0;
    // const size_t trace_size = trace.size();


    // uint64_t clk = 0;
    // while (completed < trace_size) {
    //     // ******* MODIFIED *******
    //     Logger::PrintCycle(clk);
    //     // ************************
    //     mem->ClockTick();

    //     // trace 요청 계속 주입
    //     if (trace_index < trace_size) {
    //         auto& t = trace[trace_index];
    //         if (mem->WillAcceptTransaction(t.addr, t.is_write)) {
    //             mem->AddTransaction(t.addr, t.is_write);
    //             ++trace_index;
    //         }
    //     }
    //     clk++;
    // }

    auto read_cb = [&](uint64_t addr) { return; };
    auto write_cb = [&](uint64_t addr) { return; };

    std::vector<Transaction> trace = LoadTraceFile(trace_file);    
    MemorySystem* mem = new MemorySystem(config_file, output_dir, read_cb, write_cb);
    std::deque<Transaction> channel_queues[8];

    PreprocessTrace(trace, channel_queues, mem);
    RunSimulation(mem, channel_queues, trace.size());


    // 5. 통계 출력 및 종료
    // std::cout << "[Summary] Completed in "
    //             << clk << " cycles"
    //             << std::endl;

    mem->PrintStats();
    delete mem;
    Logger::Close();
    return 0;
}
