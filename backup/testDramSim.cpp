#include <iostream>
#include <vector>
#include <iomanip>
#include <tuple>
#include <bitset>
#include "dramsim3.h"

// #define DEBUG 1

#define MEMORY_CONFIG_BASE "../configs/"
#define DDR4_CONFIG_FILE "DDR4_4Gb_x8_2666_2.ini"
// #define HBM2_CONFIG_FILE "HBM2_8Gb_x128.ini"
#define HBM2_CONFIG_FILE "VCU118_HBM2_4Gb_x128.ini"
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

uint64_t RearrangeAddress(uint64_t address) {
    uint64_t rearranged_address = 0;

    // // First Mapping
    // std::array<int, 64> bit_map = {
    //     0,  1,  2,  3,  4,  
    //     5,  29, 28, 10, 11, 14, 15, 16, 17, 
    //     6, 7, 30, 12, 18, 19, 20, 21, 22,
    //     8, 9, 31, 13, 23, 24, 25, 26, 27,
    //     32, 33, 34, 35, 36, 37, 38, 39,
    //     40, 41, 42, 43, 44, 45, 46, 47,
    //     48, 49, 50, 51, 52, 53, 54, 55,
    //     56, 57, 58, 59, 60, 61, 62, 63
    // };

    // Second Mapping
    std::array<int, 64> bit_map = {
        0,  1,  2,  3,  4,  
        28, 29, 5, 10, 14, 15, 16, 17, 18, 
        30, 6, 11, 12, 19, 20, 21, 22, 23, 
        31, 7, 8, 9, 13, 24, 25, 26, 27,
        32, 33, 34, 35, 36, 37, 38, 39,
        40, 41, 42, 43, 44, 45, 46, 47,
        48, 49, 50, 51, 52, 53, 54, 55,
        56, 57, 58, 59, 60, 61, 62, 63
    };


    for (int i = 0; i < 64; i++) {
        if (address & (1ULL << i)) {  // i번째 비트가 1이면
            rearranged_address |= (1ULL << bit_map[i]);  // 새로운 위치로 이동
        }
    }

    // Backup====
    // // [31:27] -> [31:27]
    // rearranged_address |= (address & 0b11111000000000000000000000000000);

    // // [26] -> [17]
    // rearranged_address |= ((address & 0b00000100000000000000000000000000) >> 9);

    // // [25] -> [13]
    // rearranged_address |= ((address & 0b00000010000000000000000000000000) >> 12);

    // // [24:23] -> [10:9]
    // rearranged_address |= ((address & 0b00000001100000000000000000000000) >> 14);

    // // [22:18] -> [26:22]
    // rearranged_address |= ((address & 0b00000000011111000000000000000000) << 4);

    // // [17] -> [16]
    // rearranged_address |= ((address & 0b00000000000000100000000000000000) >> 1);

    // // [16] -> [12]
    // rearranged_address |= ((address & 0b00000000000000010000000000000000) >> 4);

    // // [15:14] -> [8:7]
    // rearranged_address |= ((address & 0b00000000000000001100000000000000) >> 7);

    // // [13:10] -> [21:18]
    // rearranged_address |= ((address & 0b00000000000000000011110000000000) << 8);

    // // [9:8] -> [15:14]
    // rearranged_address |= ((address & 0b00000000000000000000001100000000) << 6);

    // // [7] -> [11]
    // rearranged_address |= ((address & 0b00000000000000000000000010000000) << 4);

    // // [6:5] -> [6:5]
    // rearranged_address |= (address & 0b00000000000000000000000001100000);

    // // [4:0] -> [4:0]
    // rearranged_address |= (address & 0b00000000000000000000000000011111);

    return rearranged_address;
}

std::vector<std::tuple<uint64_t, bool>> RearrangeTransactions(const std::vector<std::tuple<uint64_t, bool>>& transactions) {
    // 새로운 transaction 리스트
    std::vector<std::tuple<uint64_t, bool>> rearranged_transactions;

    // 각 transaction을 순회
    for (const auto& transaction : transactions) {
        uint64_t original_address;
        bool is_write;
        std::tie(original_address, is_write) = transaction; // 기존 address와 type 분리

        // Address를 rearrange
        uint64_t rearranged_address = RearrangeAddress(original_address);

        // Rearranged transaction 추가
        rearranged_transactions.emplace_back(rearranged_address, is_write);
    }

    return rearranged_transactions;
}


int main() {
    auto ddr4 = dramsim3::GetMemorySystem(DDR4_CONFIG_PATH, OUTPUT_DIR_PATH, nullptr, nullptr);
    auto hbm2 = dramsim3::GetMemorySystem(HBM2_CONFIG_PATH, OUTPUT_DIR_PATH, nullptr, nullptr);

    PrintDRAMConfigInfo(ddr4, DDR4_CONFIG_PATH);
    PrintDRAMConfigInfo(hbm2, HBM2_CONFIG_PATH);

    uint64_t start_address = 0x0;
    size_t   element_size  = 32;   // 256-bit
    size_t   num_elements  = 512;


    bool is_write = false;
    std::vector<std::tuple<uint64_t, bool>> single_transaction = {std::make_tuple(start_address, is_write)};
    std::vector<std::tuple<uint64_t, bool>> sequential_transactions = GenerateSequentialTransactions(start_address, num_elements, element_size, 64, is_write);
    std::vector<std::tuple<uint64_t, bool>> columnwise_transactions = GenerateColumnwiseTransactions(start_address, num_elements, 1<<9, element_size, 64, is_write);
    std::vector<std::tuple<uint64_t, bool>> cubewise_transactions   = GenerateColumnwiseTransactions(start_address, num_elements, 1<<18, element_size, 64, is_write);

    // single_transaction = RearrangeTransactions(single_transaction);
    // sequential_transactions = RearrangeTransactions(sequential_transactions);
    // columnwise_transactions = RearrangeTransactions(columnwise_transactions);
    // cubewise_transactions = RearrangeTransactions(cubewise_transactions);

    std::cout << "\n[OC Access]" << std::endl;
    PrintAddresses(sequential_transactions);
    std::cout << "\n[IC Access]" << std::endl;
    PrintAddresses(columnwise_transactions);
    std::cout << "\n[IR Access]" << std::endl;
    PrintAddresses(cubewise_transactions);

    // PrintDRAMConfigInfo(ddr4, DDR4_CONFIG_PATH);
    // SimulateDRAMAccess(ddr4, single_transaction);
    // SimulateDRAMAccess(ddr4, sequential_transactions);
    // SimulateDRAMAccess(ddr4, columnwise_transactions);
    // SimulateDRAMAccess(ddr4, cubewise_transactions);

    PrintDRAMConfigInfo(hbm2, HBM2_CONFIG_PATH);
    SimulateDRAMAccess(hbm2, single_transaction);
    std::cout << "\n[OC Access]" << std::endl;
    SimulateDRAMAccess(hbm2, sequential_transactions);
    std::cout << "\n[IC Access]" << std::endl;
    SimulateDRAMAccess(hbm2, columnwise_transactions);
    std::cout << "\n[IR Access]" << std::endl;
    SimulateDRAMAccess(hbm2, cubewise_transactions);

    return 0;
}
