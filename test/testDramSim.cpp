#include <iostream>
#include <vector>
#include <iomanip>
#include "dramsim3.h"

// #define DEBUG 1

#define MEMORY_CONFIG_BASE "../configs/"
#define DDR4_CONFIG_FILE "DDR4_4Gb_x8_2666_2.ini"
#define HBM2_CONFIG_FILE "HBM2_4Gb_x128.ini"
#define DDR4_CONFIG_PATH MEMORY_CONFIG_BASE DDR4_CONFIG_FILE
#define HBM2_CONFIG_PATH MEMORY_CONFIG_BASE HBM2_CONFIG_FILE
#define OUTPUT_DIR_PATH "./output"

// DRAMSim3 시뮬레이션 함수
void SimulateDRAMAccess(dramsim3::MemorySystem* dramsim, const std::vector<uint64_t>& addresses) {
    std::vector<std::pair<uint64_t, uint64_t>> completion_times; // (Address, Cycle)
    uint64_t current_clock = 0;

    // 요청 완료 콜백에서 completion_times를 업데이트
    dramsim->RegisterCallbacks(
        [&completion_times, &current_clock](uint64_t addr) { 
        #ifdef DEBUG
            std::cout << "- Read callback at address: 0x" << std::hex << addr << std::dec
                      << " at clock: " << current_clock << std::endl;
        #endif
            completion_times.emplace_back(addr, current_clock);
        },
        [](uint64_t addr) {
        #ifdef DEBUG
            std::cout << "Write callback at address: " << addr << std::endl;
        #endif
        }
    );

    size_t address_index = 0;
   // 클럭 틱 진행 및 트랜잭션 추가
    while (completion_times.size() < addresses.size()) {
        // Add transactions only if there is space in the queue
        if (address_index < addresses.size()) {
            if (dramsim->WillAcceptTransaction(addresses[address_index], false)) { // is_write = false
                dramsim->AddTransaction(addresses[address_index], false);
                address_index++; // 다음 주소로 이동
            }
        }

        // 진행 중인 트랜잭션 처리
        dramsim->ClockTick();
        current_clock++;
    }

    // 시뮬레이션 결과 출력    double elapsed_time_ns = clock_cycles * tCK;
    double tCK = dramsim->GetTCK();
    double elapsed_time_ns = current_clock * tCK;

    std::cout << "Elapsed Time: " << elapsed_time_ns << " ns (" << current_clock << " tCK)" << std::endl;
#ifdef DEBUG
    dramsim->PrintStats();
#endif    
}

std::vector<uint64_t> generate_sequential_addresses(uint64_t start_address, size_t num_elements, size_t element_size, size_t burst_size) {
    std::vector<uint64_t> addresses;
    size_t data_size = element_size * num_elements;
    size_t total_requests = data_size / burst_size;
    for (size_t i = 0; i < total_requests; i++) addresses.push_back(start_address + i * burst_size);
    return addresses;
}

std::vector<uint64_t> generate_columnwise_addresses(uint64_t start_address, size_t num_elements, size_t stride, size_t element_size, size_t burst_size) {
    std::vector<uint64_t> addresses;
    
    // If many elements in a burst, modify stride & num_elements
    size_t elements_in_burst = (burst_size / (element_size * stride)) + (burst_size % (element_size * stride) ? 1 :0);
    stride = stride * elements_in_burst;
    num_elements = (num_elements / elements_in_burst) + (num_elements % elements_in_burst ? 1 :0);

    // 
    size_t requests_per_element = (element_size / burst_size) + (element_size % burst_size ? 1 : 0);
    size_t stride_size = stride * element_size;

    // std::cout << "=======================================" << std::endl;
    // std::cout << "elements_in_burst : " << elements_in_burst << std::endl;
    // std::cout << "num_elements : " << num_elements << std::endl;
    // std::cout << "stride : " << stride << std::endl;
    // std::cout << "requests_per_element : " << requests_per_element << std::endl;
    // std::cout << "stride_size : " << stride_size << std::endl;
    // std::cout << "=======================================" << std::endl;

    for (size_t row = 0; row < num_elements; row++) {
        for(size_t burst = 0; burst < requests_per_element ; burst++){
            addresses.push_back(start_address + row * stride_size + burst);
        }
    }
    return addresses;
}

void print_addresses(const std::vector<uint64_t>& addresses) {
    std::cout << "Generated Addresses:" << std::endl;

    // 처음 4개의 주소 출력
    size_t total_addresses = addresses.size();
    for (size_t i = 0; i < 4 && i < total_addresses; ++i) {
        std::cout << "Address [" << i << "]: 0x" 
                  << std::hex << std::setw(8) << std::setfill('0') << addresses[i] 
                  << std::dec << std::endl; // Reset to decimal
    }

    // 마지막 4개의 주소 출력
    if (total_addresses > 4) {
        std::cout << "..." << std::endl; // 생략된 부분 표시
        for (size_t i = total_addresses - 4; i < total_addresses; ++i) {
            std::cout << "Address [" << i << "]: 0x" 
                      << std::hex << std::setw(8) << std::setfill('0') << addresses[i] 
                      << std::dec << std::endl; // Reset to decimal
        }
    }
}

void PrintDRAMConfigInfo(dramsim3::MemorySystem* memory, const std::string& config_file) {
    std::cout << std::endl;
    std::cout << "[i] Using Config File: " << config_file << std::endl;
    std::cout << "- BusBits     : " << memory->GetBusBits() << std::endl;
    std::cout << "- BurstLength : " << memory->GetBurstLength() << std::endl;
    size_t burst_size = (memory->GetBusBits() / 8) * memory->GetBurstLength();
    std::cout << "- Burst Size  : " << burst_size << " bytes" << std::endl;
}

int main() {
    // DRAMSim3 MemorySystem 생성
    auto ddr4 = dramsim3::GetMemorySystem(
        DDR4_CONFIG_PATH,    // Config File Path
        OUTPUT_DIR_PATH,       // Output Directory
        nullptr, nullptr       // 콜백은 SimulateDRAMAccess에서 등록
    );

    auto hbm2 = dramsim3::GetMemorySystem(
        HBM2_CONFIG_PATH,    // Config File Path
        OUTPUT_DIR_PATH,       // Output Directory
        nullptr, nullptr       // 콜백은 SimulateDRAMAccess에서 등록
    );

    PrintDRAMConfigInfo(ddr4, DDR4_CONFIG_PATH);
    PrintDRAMConfigInfo(hbm2, HBM2_CONFIG_PATH);

    // 주소 리스트 정의
    uint64_t start_address = 0x0;
    size_t element_size = 32;   // 256-bit
    size_t num_elements = 512;
    
    // std::cout << "\n[One Access]" << std::endl;
    // std::vector<uint64_t> test_address;
    // test_address.push_back(0x0);
    // SimulateDRAMAccess(ddr4, test_address);
    // SimulateDRAMAccess(hbm2, test_address);

    // Sequential Access
    std::cout << "\n[Sequential Access]" << std::endl;
    auto sequential_addresses = generate_sequential_addresses(start_address, num_elements, element_size, 64);  
    // print_addresses(sequential_addresses);          
    SimulateDRAMAccess(ddr4, sequential_addresses);
    SimulateDRAMAccess(hbm2, sequential_addresses);

    // Column-wise Access
    std::cout << "\n[Column-wise Access]" << std::endl;
    size_t stride = 1<<9;
    auto columnwise_addresses = generate_columnwise_addresses(start_address, num_elements, stride, element_size, 64);
    // print_addresses(columnwise_addresses);
    SimulateDRAMAccess(ddr4, columnwise_addresses);
    SimulateDRAMAccess(hbm2, columnwise_addresses);

    
    // Cube-wise Access
    std::cout << "\n[Cube-wise Access]" << std::endl;
    stride = 1<<18;
    auto cubenwise_addresses = generate_columnwise_addresses(start_address, num_elements, stride, element_size, 64);
    // print_addresses(cubenwise_addresses);
    SimulateDRAMAccess(ddr4, cubenwise_addresses);
    SimulateDRAMAccess(hbm2, cubenwise_addresses);

    return 0;
}
