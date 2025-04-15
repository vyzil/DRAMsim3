#include "dramsim3.h"  // MemorySystem 인터페이스만 사용
#include <iostream>
#include <fstream>
#include <vector>
#include <tuple>
#include <map>

using namespace dramsim3;

struct Transaction {
    uint64_t addr;
    bool is_write;
    uint64_t issue_cycle;
    uint64_t complete_cycle;
    bool completed = false;
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: ./sim <config.ini> <trace.txt>" << std::endl;
        return 1;
    }

    std::string config_file = "configs/VCU118_HBM2_4Gb_x128.ini";
    std::string trace_file = "test.trace";

    std::vector<Transaction> transactions;

    // 1. Trace 파싱
    {
        std::ifstream fin(trace_file);
        std::string rw;
        uint64_t addr;

        while (fin >> std::hex >> addr >> rw) {
            transactions.push_back({addr, rw == "W", 0, 0, false});
        }
    }

    // 2. MemorySystem 생성
    MemorySystem mem(config_file);

    // 3. 주소 → index 매핑해서 latency 측정
    std::map<uint64_t, std::vector<size_t>> addr_to_indices;

    for (size_t i = 0; i < transactions.size(); ++i) {
        addr_to_indices[transactions[i].addr].push_back(i);
    }

    // 4. 콜백 등록
    mem.RegisterCallbacks(
        [&](uint64_t addr) {
            for (auto idx : addr_to_indices[addr]) {
                if (!transactions[idx].completed && !transactions[idx].is_write) {
                    transactions[idx].complete_cycle = transactions[idx].issue_cycle;
                    transactions[idx].completed = true;
                    break;
                }
            }
        },
        [&](uint64_t addr) {
            for (auto idx : addr_to_indices[addr]) {
                if (!transactions[idx].completed && transactions[idx].is_write) {
                    transactions[idx].complete_cycle = transactions[idx].issue_cycle;
                    transactions[idx].completed = true;
                    break;
                }
            }
        }
    );

    // 5. 시뮬레이션 루프
    uint64_t cycle = 0;
    size_t index = 0;
    size_t pending = 0;

    while (index < transactions.size() || pending > 0) {
        // 요청 투입
        if (index < transactions.size()) {
            auto& t = transactions[index];
            if (mem.WillAcceptTransaction(t.addr, t.is_write)) {
                t.issue_cycle = cycle;
                mem.AddTransaction(t.addr, t.is_write);
                ++index;
                ++pending;
            }
        }

        mem.ClockTick();
        ++cycle;

        // 완료 검사
        for (auto& t : transactions) {
            if (t.completed && t.complete_cycle == 0) {
                t.complete_cycle = cycle;
                --pending;
            }
        }
    }

    // 6. 결과 출력
    uint64_t total_latency = 0;
    for (auto& t : transactions) {
        uint64_t latency = t.complete_cycle - t.issue_cycle;
        total_latency += latency;
        std::cout << (t.is_write ? "W" : "R") << " 0x" << std::hex << t.addr
                  << std::dec << " latency: " << latency << " cycles" << std::endl;
    }

    std::cout << "Average latency: "
              << (double)total_latency / transactions.size()
              << " cycles" << std::endl;

    mem.PrintStats();
    return 0;
}
