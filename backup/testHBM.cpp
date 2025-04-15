#include <iostream>
#include <vector>
#include <iomanip>
#include <tuple>
#include <bitset>
#include "dramsim3.h"

// #define DEBUG 1

#define MEMORY_CONFIG_BASE "../configs/"
// #define HBM2_CONFIG_FILE "HBM2_8Gb_x128.ini"
#define HBM2_CONFIG_FILE "VCU118_HBM2_4Gb_x128.ini"
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

    // Second Mapping (address_mapping = rorabgbachco)
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

    // // FPGA HBM2E
    // std::array<int, 64> bit_map = {
    //     0,  1,  2,  3,  4, // offset
    //     5, 6, 7, 8, 9,     // column
    //     28, 29, 30, 31,    // channel
    //     10, 11, 12 ,13,    // bank group & bank
    //     14, 15, 16, 17, 18, 19, 20, 
    //     21, 22, 23, 24, 25, 26, 27,        // Row
    //     32, 33, 34, 35, 36, 37, 38, 39,
    //     40, 41, 42, 43, 44, 45, 46, 47,
    //     48, 49, 50, 51, 52, 53, 54, 55,
    //     56, 57, 58, 59, 60, 61, 62, 63
    // };

    for (int i = 0; i < 64; i++) {
        if (address & (1ULL << i)) {  // i번째 비트가 1이면
            rearranged_address |= (1ULL << bit_map[i]);  // 새로운 위치로 이동
        }
    }


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

// IC-wise NTT의 주소 생성 함수 (parallel_for 내에서 처리할 512개 데이터를 한 묶음으로)
std::vector<std::tuple<uint64_t, bool>> GenerateICWiseNTTAddresses(
    uint64_t startaddress) {
    
    std::vector<std::tuple<uint64_t, bool>> transactions;

    for(size_t i = 0; i < (1 << 5); i++){
        for(size_t channel = 0; channel < (1 << 4); channel++){
            uint64_t addr = channel * (1 << 27) + (startaddress + i * (1 << 9) * (1 << 4)) << 5;
            transactions.emplace_back(addr, false);
        }
    }
    return transactions;
}

std::vector<std::tuple<uint64_t, bool>> GenerateStridedAddresses(
    uint64_t startaddress, uint64_t stride, uint64_t elements, bool is_write) {
    
    std::vector<std::tuple<uint64_t, bool>> transactions;


    for(uint64_t i = 0; i < elements; i++){
        uint64_t addr = startaddress + (i*stride << 5);
        transactions.emplace_back(addr, false);
    }
    return transactions;
}


int main() {
    auto hbm2 = dramsim3::GetMemorySystem(HBM2_CONFIG_PATH, OUTPUT_DIR_PATH, nullptr, nullptr);

    PrintDRAMConfigInfo(hbm2, HBM2_CONFIG_PATH);

    uint64_t startaddress = 0x0;
    uint64_t   elements  = 512;
    bool is_write = false;
    uint64_t stride = (1 << 18);

    // // 01 Strided Test
    // for(stride = 1; (stride*(elements-1)) < ((uint64_t )1<<(32)); stride <<= 1){
    //     printf("Stride : %ld\n", stride);
    //     is_write = false;
    //     std::vector<std::tuple<uint64_t, bool>> read_transactions = GenerateStridedAddresses(startaddress, stride, elements, is_write);
    //     SimulateDRAMAccess(hbm2, read_transactions);
    //     is_write = true;
    //     std::vector<std::tuple<uint64_t, bool>> write_transactions = GenerateStridedAddresses(startaddress, stride, elements, is_write);
    //     SimulateDRAMAccess(hbm2, write_transactions);
    // }

    // // 02 Rearrange Test
    // for(stride = 1; (stride*(elements-1)) < (1<<(32-5)); stride <<= 1){
    //     printf("Stride : %ld\n", stride);
    //     is_write = false;
    //     std::vector<std::tuple<uint64_t, bool>> read_transactions = GenerateStridedAddresses(startaddress, stride, elements, is_write);
    //     SimulateDRAMAccess(hbm2, read_transactions);
    //     SimulateDRAMAccess(hbm2, RearrangeTransactions(read_transactions));
    //     is_write = false;
    //     std::vector<std::tuple<uint64_t, bool>> write_transactions = GenerateStridedAddresses(startaddress, stride, elements, is_write);
    //     SimulateDRAMAccess(hbm2, write_transactions);
    //     SimulateDRAMAccess(hbm2, RearrangeTransactions(write_transactions));
    // }

    // // 03 Test : OC/IC/IR : Read + Read(Arrange) + Write + Write(Arrange)
    // std::vector<std::tuple<uint64_t, bool>> read_transactions;
    // std::vector<std::tuple<uint64_t, bool>> write_transactions;
    
    // std::cout << "\n[OC Access]" << std::endl;
    // stride = (1 << 18);
    // printf("Stride : %ld\n", stride);
    // is_write = false;
    // read_transactions = GenerateStridedAddresses(startaddress, stride, elements, is_write);
    // SimulateDRAMAccess(hbm2, read_transactions);
    // SimulateDRAMAccess(hbm2, RearrangeTransactions(read_transactions));
    // is_write = true;
    // write_transactions = GenerateStridedAddresses(startaddress, stride, elements, is_write);
    // SimulateDRAMAccess(hbm2, write_transactions);
    // SimulateDRAMAccess(hbm2, RearrangeTransactions(write_transactions));

    // std::cout << "\n[IC Access]" << std::endl;
    // stride = (1 << 9);
    // printf("Stride : %ld\n", stride);
    // is_write = false;
    // read_transactions = GenerateStridedAddresses(startaddress, stride, elements, is_write);
    // SimulateDRAMAccess(hbm2, read_transactions);
    // SimulateDRAMAccess(hbm2, RearrangeTransactions(read_transactions));
    // is_write = true;
    // write_transactions = GenerateStridedAddresses(startaddress, stride, elements, is_write);
    // SimulateDRAMAccess(hbm2, write_transactions);
    // SimulateDRAMAccess(hbm2, RearrangeTransactions(write_transactions));

    // std::cout << "\n[IR Access]" << std::endl;
    // stride = 1;
    // printf("Stride : %ld\n", stride);
    // is_write = false;
    // read_transactions = GenerateStridedAddresses(startaddress, stride, elements, is_write);
    // SimulateDRAMAccess(hbm2, read_transactions);
    // SimulateDRAMAccess(hbm2, RearrangeTransactions(read_transactions));
    // is_write = true;
    // write_transactions = GenerateStridedAddresses(startaddress, stride, elements, is_write);
    // SimulateDRAMAccess(hbm2, write_transactions);
    // SimulateDRAMAccess(hbm2, RearrangeTransactions(write_transactions));

    // // 04 Rearrange Test
    // for(stride = (uint64_t)1; (stride*(elements-1)) < ((uint64_t)1<<32); stride <<= (uint64_t)1){
    //     printf("Stride : %ld\n", stride);
    //     is_write = false;
    //     std::vector<std::tuple<uint64_t, bool>> read_transactions = GenerateStridedAddresses(startaddress, stride, elements, is_write);
    //     // PrintAddresses(read_transactions);
    //     SimulateDRAMAccess(hbm2, read_transactions);
    // }

    
    // IC_wise first
    for (size_t subtile = 0; subtile < (1 << 4); subtile++) {  // 2^4 개의 subtile
        for (size_t outer_row_offset = 0; outer_row_offset < ((1 << 5)); outer_row_offset++) {
            for (size_t inner_col = 0; inner_col < (1 << 9); inner_col++) {
                startaddress = outer_row_offset * (1 << 18) + subtile * (1 << 9) + inner_col;
                auto ICwise_transactions = GenerateICWiseNTTAddresses(startaddress);
                // PrintAddresses(ICwise_transactions);
                SimulateDRAMAccess(hbm2, ICwise_transactions);
            }
        }
    }

    return 0;
}
