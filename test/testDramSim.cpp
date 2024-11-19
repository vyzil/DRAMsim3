#include <iostream>
#include <vector>
#include <iomanip>
#include <tuple>
#include "dramsim3.h"

// #define DEBUG 1

#define MEMORY_CONFIG_BASE "../configs/"
#define DDR4_CONFIG_FILE "DDR4_4Gb_x8_2666_2.ini"
#define HBM2_CONFIG_FILE "HBM2_4Gb_x128.ini"
#define DDR4_CONFIG_PATH MEMORY_CONFIG_BASE DDR4_CONFIG_FILE
#define HBM2_CONFIG_PATH MEMORY_CONFIG_BASE HBM2_CONFIG_FILE
#define OUTPUT_DIR_PATH "./output"

// Print DRAM Configuration
void PrintDRAMConfigInfo(dramsim3::MemorySystem* memory, const std::string& config_file) {
    std::cout << std::endl;
    std::cout << "[i] Using Config File: " << config_file << std::endl;
    std::cout << "- BusBits     : " << memory->GetBusBits() << std::endl;
    std::cout << "- BurstLength : " << memory->GetBurstLength() << std::endl;
    size_t burst_size = (memory->GetBusBits() / 8) * memory->GetBurstLength();
    std::cout << "- Burst Size  : " << burst_size << " bytes" << std::endl;
}

// Print Address List (First and Last 4 Addresses)
void PrintAddresses(const std::vector<std::tuple<uint64_t, bool>>& transactions) {
    std::cout << "Generated Transactions:" << std::endl;

    size_t total_transactions = transactions.size();
    for (size_t i = 0; i < 4 && i < total_transactions; ++i) {
        auto [address, is_write] = transactions[i];
        std::cout << "Transaction [" << i << "]: Address 0x"
                  << std::hex << std::setw(8) << std::setfill('0') << address
                  << " | Type: " << (is_write ? "Write" : "Read") << std::dec << std::endl;
    }

    if (total_transactions > 4) {
        std::cout << "..." << std::endl; // Indicate skipped addresses
        for (size_t i = total_transactions - 4; i < total_transactions; ++i) {
            auto [address, is_write] = transactions[i];
            std::cout << "Transaction [" << i << "]: Address 0x"
                      << std::hex << std::setw(8) << std::setfill('0') << address
                      << " | Type: " << (is_write ? "Write" : "Read") << std::dec << std::endl;
        }
    }
}

// Generate Sequential Transactions (Read/Write)
std::vector<std::tuple<uint64_t, bool>> GenerateSequentialTransactions(
    uint64_t start_address, size_t num_elements, size_t element_size, size_t burst_size, bool is_write) {
    std::vector<std::tuple<uint64_t, bool>> transactions;
    size_t data_size = element_size * num_elements;
    size_t total_requests = data_size / burst_size;

    for (size_t i = 0; i < total_requests; ++i) {
        transactions.emplace_back(start_address + i * burst_size, is_write);
    }
    return transactions;
}

// Generate Column-wise Transactions (Read/Write)
std::vector<std::tuple<uint64_t, bool>> GenerateColumnwiseTransactions(
    uint64_t start_address, size_t num_elements, size_t stride, size_t element_size, size_t burst_size, bool is_write) {
    std::vector<std::tuple<uint64_t, bool>> transactions;

    size_t elements_in_burst = (burst_size / (element_size * stride)) + (burst_size % (element_size * stride) ? 1 : 0);
    stride *= elements_in_burst;
    num_elements = (num_elements / elements_in_burst) + (num_elements % elements_in_burst ? 1 : 0);

    size_t requests_per_element = (element_size / burst_size) + (element_size % burst_size ? 1 : 0);
    size_t stride_size = stride * element_size;

    for (size_t row = 0; row < num_elements; ++row) {
        for (size_t burst = 0; burst < requests_per_element; ++burst) {
            transactions.emplace_back(start_address + row * stride_size + burst, is_write);
        }
    }
    return transactions;
}

// RegisterCallbacks
void RegisterCallbacks(
    dramsim3::MemorySystem* dramsim,
    std::vector<std::tuple<uint64_t, bool, uint64_t>>& completion_times, // (Address, Is_Write, Clock)
    uint64_t& current_clock) {

    // Read Callback
    auto read_callback = [&completion_times, &current_clock](uint64_t addr) {
    #ifdef DEBUG
        std::cout << "- Read callback at address: 0x" << std::hex << addr << std::dec
                  << " at clock: " << current_clock << std::endl;
    #endif
        completion_times.emplace_back(addr, false, current_clock); // false = Read
    };

    // Write Callback
    auto write_callback = [&completion_times, &current_clock](uint64_t addr) {
    #ifdef DEBUG
        std::cout << "- Write callback at address: 0x" << std::hex << addr << std::dec
                  << " at clock: " << current_clock << std::endl;
    #endif
        completion_times.emplace_back(addr, true, current_clock); // true = Write
    };

    // 등록
    dramsim->RegisterCallbacks(read_callback, write_callback);
}

// Simulate DRAM Access
void SimulateDRAMAccess(
    dramsim3::MemorySystem* dramsim,
    const std::vector<std::tuple<uint64_t, bool>>& transactions) { // (Address, Is_Write)
    std::vector<std::tuple<uint64_t, bool, uint64_t>> completion_times; // (Address, Is_Write, Clock)
    uint64_t current_clock = 0;

    RegisterCallbacks(dramsim, completion_times, current_clock);

    size_t transaction_index = 0;

    while (completion_times.size() < transactions.size()) {
        if (transaction_index < transactions.size()) {
            auto [address, is_write] = transactions[transaction_index];
            if (dramsim->WillAcceptTransaction(address, is_write)) {
                dramsim->AddTransaction(address, is_write);
                transaction_index++;
            }
        }
        dramsim->ClockTick();
        current_clock++;
    }

    double tCK = dramsim->GetTCK();
    double elapsed_time_ns = current_clock * tCK;
    std::cout << "Elapsed Time: " << elapsed_time_ns << " ns (" << current_clock << " tCK)" << std::endl;
}

int main() {
    auto ddr4 = dramsim3::GetMemorySystem(DDR4_CONFIG_PATH, OUTPUT_DIR_PATH, nullptr, nullptr);
    auto hbm2 = dramsim3::GetMemorySystem(HBM2_CONFIG_PATH, OUTPUT_DIR_PATH, nullptr, nullptr);

    PrintDRAMConfigInfo(ddr4, DDR4_CONFIG_PATH);
    PrintDRAMConfigInfo(hbm2, HBM2_CONFIG_PATH);

    uint64_t start_address = 0x0;
    size_t   element_size  = 32;   // 256-bit
    size_t   num_elements  = 512;


    bool is_write = true;
    std::vector<std::tuple<uint64_t, bool>> single_transaction = {std::make_tuple(start_address, is_write)};
    std::vector<std::tuple<uint64_t, bool>> sequential_transactions = GenerateSequentialTransactions(start_address, num_elements, element_size, 64, is_write);
    std::vector<std::tuple<uint64_t, bool>> columnwise_transactions = GenerateColumnwiseTransactions(start_address, num_elements, 1<<9, element_size, 64, is_write);
    std::vector<std::tuple<uint64_t, bool>> cubewise_transactions   = GenerateColumnwiseTransactions(start_address, num_elements, 1<<18, element_size, 64, is_write);


    std::cout << "\n[Sequential Access]" << std::endl;
    PrintAddresses(sequential_transactions);
    std::cout << "\n[Column-wise Access]" << std::endl;
    PrintAddresses(columnwise_transactions);
    std::cout << "\n[Cube-wise Access]" << std::endl;
    PrintAddresses(cubewise_transactions);

    PrintDRAMConfigInfo(ddr4, DDR4_CONFIG_PATH);
    SimulateDRAMAccess(ddr4, single_transaction);
    SimulateDRAMAccess(ddr4, sequential_transactions);
    SimulateDRAMAccess(ddr4, columnwise_transactions);
    SimulateDRAMAccess(ddr4, cubewise_transactions);

    PrintDRAMConfigInfo(hbm2, HBM2_CONFIG_PATH);
    SimulateDRAMAccess(hbm2, single_transaction);
    SimulateDRAMAccess(hbm2, sequential_transactions);
    SimulateDRAMAccess(hbm2, columnwise_transactions);
    SimulateDRAMAccess(hbm2, cubewise_transactions);

    // for (auto& memory : {std::make_pair(ddr4, "DDR4"), std::make_pair(hbm2, "HBM2")}) {
    // std::cout << "\n[Performing Stride Tests on " << memory.second << "]" << std::endl;

    //     bool is_write = false;
    //     for (int k = 0; k <= 28; ++k) {
    //         size_t stride = static_cast<size_t>(1) << k; // Stride = 2^k
    //         std::vector<std::tuple<uint64_t, bool>> stride_transactions = 
    //             GenerateColumnwiseTransactions(start_address, 8, stride, element_size, 64, is_write);

    //         // 결과 출력
    //         std::cout << "\n-- Testing with Stride = 2^" << k << " (" << stride << ")" << std::endl;
    //         PrintAddresses(stride_transactions);
    //         SimulateDRAMAccess(memory.first, stride_transactions);
    //     }
    // }

    return 0;
}
