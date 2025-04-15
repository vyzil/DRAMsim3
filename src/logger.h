// logger.h
#pragma once

#include <fstream>
#include <string>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <optional>
#include "common.h"

namespace dramsim3 {

// Global print flags (set from main)
extern bool g_print_cycle;
extern bool g_print_issue;
extern bool g_print_return;

class Logger {
   public:
    static void Init(const std::string& dir = "output/");
    static void PrintCycle(uint64_t clk);
    static void PrintIssue(uint64_t clk, const Command& cmd);
    static void PrintReturn(uint64_t clk, const Transaction& trans);
    static void Close();

   private:
    static std::ofstream log_all_;
    static std::ofstream log_issue_;
    static std::ofstream log_return_;
    
    static uint64_t current_cycle_;
    static std::optional<uint64_t> last_log_cycle_;
    static std::optional<uint64_t> last_issue_cycle_;
    static std::optional<uint64_t> last_return_cycle_;

    
    static void EmitCycleIfNeeded(std::ostream& out, std::optional<uint64_t>& last_cycle);
};

}  // namespace dramsim3
