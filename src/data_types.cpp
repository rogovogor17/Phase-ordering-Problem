#include "data_types.hpp"

#include <iomanip>
#include <numeric>
#include <sstream>

namespace phaseordering {

std::string algorithmTypeToString(AlgorithmType type) {
    switch (type) {
        case AlgorithmType::RandomSearch:
            return "RandomSearch";
        case AlgorithmType::HillClimbing:
            return "HillClimbing";
        case AlgorithmType::SimulatedAnnealing:
            return "SimulatedAnnealing";
    }
    return "Unknown";
}

std::string costModelTypeToString(CostModelType type) {
    switch (type) {
        case CostModelType::Weighted:
            return "Weighted";
        case CostModelType::InstructionCount:
            return "InstructionCount";
        case CostModelType::Runtime:
            return "Runtime";
    }
    return "Unknown";
}

std::string IRMetrics::toString() const {
    std::ostringstream oss;
    oss << "IRMetrics {\n";
    oss << "  totalInstructions: " << totalInstructions << "\n";
    oss << "  totalBasicBlocks: " << totalBasicBlocks << "\n";
    oss << "  totalFunctions: " << totalFunctions << "\n";
    oss << "  avgBasicBlockSize: " << std::fixed << std::setprecision(2)
        << avgBasicBlockSize << "\n";
    oss << "  maxBlockDepth: " << maxBlockDepth << "\n";
    oss << "  memoryOps: " << memoryOps << "\n";
    oss << "  branchOps: " << branchOps << "\n";
    oss << "  callOps: " << callOps << "\n";
    oss << "  arithmeticOps: " << arithmeticOps << "\n";
    oss << "  executionTimeMs: " << std::fixed << std::setprecision(3)
        << executionTimeMs << "\n";
    oss << "}";
    return oss.str();
}

OptSequence::OptSequence(const std::vector<std::string>& passes)
    : passes_(passes) {}

void OptSequence::add(const std::string& pass) { passes_.push_back(pass); }

void OptSequence::removeAt(int index) {
    if (index >= 0 && index < static_cast<int>(passes_.size())) {
        passes_.erase(passes_.begin() + index);
    }
}

int OptSequence::size() const { return static_cast<int>(passes_.size()); }

bool OptSequence::empty() const { return passes_.empty(); }

const std::vector<std::string>& OptSequence::passes() const { return passes_; }

std::vector<std::string>& OptSequence::passes() { return passes_; }

std::string OptSequence::toString() const {
    if (passes_.empty()) return "<empty>";
    std::string result;
    for (size_t i = 0; i < passes_.size(); ++i) {
        if (i > 0) result += " -> ";
        result += passes_[i];
    }
    return result;
}

}  // namespace phaseordering
