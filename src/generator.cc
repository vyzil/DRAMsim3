#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <cmath>
#include <cstdlib>

void GenerateTrace(const std::string& output_path,
                   size_t element_size,
                   size_t start_idx,
                   size_t stride,
                   size_t count,
                   bool   is_write) {
    std::ofstream fout(output_path);
    if (!fout) {
        std::cerr << "Failed to open output file: " << output_path << std::endl;
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        uint64_t addr = static_cast<uint64_t>(start_idx + i * stride) * element_size;
        fout << "0x" << std::hex << std::setw(8) << std::setfill('0') << addr
             << " " << (is_write ? "W" : "R") << "\n";
    }

    std::cout << "[Generator] Trace written to: " << output_path << "\n";
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0]
                  << " <start_idx> <stride_exp> <count> <is_write: 0|1>\n";
        return 1;
    }

    size_t element_size = 32;
    size_t start_idx = std::stoull(argv[1]);
    size_t stride_exp = std::stoull(argv[2]);
    size_t stride = 1ULL << stride_exp;  // 2^stride_exp
    size_t count = std::stoull(argv[3]);
    bool is_write = std::stoi(argv[4]) != 0;

    GenerateTrace("traces/test.trace", element_size, start_idx, stride, count, is_write);
    return 0;
}
