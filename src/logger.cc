#include "logger.h"

namespace dramsim3 {

// Static members
std::ofstream Logger::log_all_;
std::ofstream Logger::log_issue_;
std::ofstream Logger::log_return_;

uint64_t Logger::current_cycle_ = 0;
std::optional<uint64_t> Logger::last_log_cycle_ = std::nullopt;
std::optional<uint64_t> Logger::last_issue_cycle_ = std::nullopt;
std::optional<uint64_t> Logger::last_return_cycle_ = std::nullopt;

void Logger::Init(const std::string& dir) {
    std::filesystem::create_directories(dir);
    log_all_.open(dir + "/log.txt");
    log_issue_.open(dir + "/issue.txt");
    log_return_.open(dir + "/return.txt");
}

void Logger::Close() {
    log_all_.close();
    log_issue_.close();
    log_return_.close();
}

void Logger::PrintCycle(uint64_t clk) {
    current_cycle_ = clk;
    if (g_print_cycle) {
        std::ostringstream oss;
        oss << "[+] " << std::setw(5) << std::left << clk;
        std::cout << "\033[36m" << oss.str() << "\033[0m\r" << std::flush;
    }
}

void Logger::EmitCycleIfNeeded(std::ostream& out, std::optional<uint64_t>& last_cycle) {
    if (!last_cycle.has_value() || last_cycle.value() != current_cycle_) {
        out << "[+] " << std::setw(5) << std::left << current_cycle_;
        last_cycle = current_cycle_;
    } else {
        out << std::setw(8) << " "; // 공백 맞추기
    }
}


void Logger::PrintIssue(uint64_t clk, const Command& cmd) {
    std::ostringstream line;
    std::string cmd_str;

    switch (cmd.cmd_type) {
        case CommandType::READ: cmd_str = "READ"; break;
        case CommandType::READ_PRECHARGE: cmd_str = "READ_PRECHARGE"; break;
        case CommandType::WRITE: cmd_str = "WRITE"; break;
        case CommandType::WRITE_PRECHARGE: cmd_str = "WRITE_PRECHARGE"; break;
        case CommandType::ACTIVATE: cmd_str = "ACTIVATE"; break;
        case CommandType::PRECHARGE: cmd_str = "PRECHARGE"; break;
        case CommandType::REFRESH_BANK: cmd_str = "REFRESH_BANK"; break;
        case CommandType::REFRESH: cmd_str = "REFRESH"; break;
        case CommandType::SREF_ENTER: cmd_str = "SREF_ENTER"; break;
        case CommandType::SREF_EXIT: cmd_str = "SREF_EXIT"; break;
        default: cmd_str = "INVALID";
    }

    line << "\t\t[*] (Issue) "
         << " Type: " << std::left << std::setw(15) << cmd_str
         << " | Addr: 0x" << std::right << std::hex << std::setw(8) << std::setfill('0') << cmd.hex_addr << std::dec
        << " -> Channel: " << std::setw(2) << std::setfill(' ') << cmd.addr.channel
        << ", BG: " << std::setw(1) << cmd.addr.bankgroup
        << ", Bank: " << std::setw(1) << cmd.addr.bank
        << ", Row: " << std::setw(5) << cmd.addr.row
        << ", Col: " << std::setw(2) << cmd.addr.column;

    std::string log_line = line.str() + "\n";

    if (g_print_issue) std::cout << "\033[32m" << log_line << "\033[0m";
    EmitCycleIfNeeded(log_all_, last_log_cycle_);
    EmitCycleIfNeeded(log_issue_, last_issue_cycle_);
    log_all_ << log_line;
    log_issue_ << log_line;
}

void Logger::PrintReturn(uint64_t clk, const Transaction& trans) {
    std::ostringstream line;
    line << "\t\t[=] (Return)"
         << " Type: " << std::left << std::setw(15) << std::setfill(' ') << (trans.is_write ? "WRITE" : "READ")
         << " | Addr: 0x" << std::right << std::hex << std::setw(8) << std::setfill('0') << trans.addr << std::dec
         << " | Added: " << std::setw(5) << std::setfill(' ') << trans.added_cycle
         << ", Done: " << std::setw(5) << std::setfill(' ') << trans.complete_cycle
         << ", Latency: " <<  std::setw(5) << std::setfill(' ') << (trans.complete_cycle - trans.added_cycle);

    std::string log_line = line.str() + "\n";

    if (g_print_return) std::cout << "\033[34m" << log_line << "\033[0m";
    EmitCycleIfNeeded(log_all_, last_log_cycle_);
    EmitCycleIfNeeded(log_return_, last_return_cycle_);
    log_all_ << log_line;
    log_return_ << log_line;
}

}  // namespace dramsim3
