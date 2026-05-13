#include "ir_analyzer.hpp"

#include <sstream>

namespace phaseordering {

IRMetrics IRAnalyzer::analyze(const std::string& ir) const {
    IRMetrics metrics;

    if (ir.size() < 20) {
        metrics.totalInstructions = -1;
        return metrics;
    }

    size_t definePos = ir.find("define ");
    if (definePos == std::string::npos) {
        metrics.totalInstructions = -1;
        return metrics;
    }

    metrics.totalFunctions = countFunctions(ir);
    if (metrics.totalFunctions == 0) {
        metrics.totalInstructions = -1;
        return metrics;
    }

    metrics.totalInstructions = countInstructions(ir);
    metrics.totalBasicBlocks = countBasicBlocks(ir);
    metrics.memoryOps = countMemoryOps(ir);
    metrics.branchOps = countBranchOps(ir);
    metrics.callOps = countCallOps(ir);
    metrics.arithmeticOps = countArithmeticOps(ir);
    metrics.avgBasicBlockSize =
        metrics.totalBasicBlocks > 0
            ? static_cast<double>(metrics.totalInstructions) /
                  metrics.totalBasicBlocks
            : 0.0;
    metrics.maxBlockDepth = 0;
    metrics.executionTimeMs = 0.0;
    return metrics;
}

int IRAnalyzer::countOccurrences(const std::string& ir,
                                 const std::string& pattern) const {
    if (pattern.empty()) return 0;
    int count = 0;
    size_t pos = 0;
    while ((pos = ir.find(pattern, pos)) != std::string::npos) {
        count++;
        pos += pattern.length();
    }
    return count;
}

int IRAnalyzer::countFunctions(const std::string& ir) const {
    int count = 0;
    size_t pos = 0;
    while ((pos = ir.find("define ", pos)) != std::string::npos) {
        count++;
        pos = ir.find('{', pos);
        if (pos == std::string::npos) break;
        pos++;
    }
    return count;
}

bool isInstructionLine(const std::string& line) {
    if (line.empty()) return false;

    char c = line[0];

    if (c == ';' || c == '!' || c == '{' || c == '}') return false;

    if (line.rfind("define ", 0) == 0) return false;
    if (line.rfind("declare ", 0) == 0) return false;
    if (line.rfind("source_filename", 0) == 0) return false;
    if (line.rfind("target ", 0) == 0) return false;
    if (line.rfind("attributes ", 0) == 0) return false;
    if (line.rfind(";", 0) == 0) return false;
    if (line.rfind("label ", 0) == 0) return false;
    if (line.rfind("unique ", 0) == 0) return false;
    if (line.rfind("dso_local", 0) == 0) return false;
    if (line.rfind("private ", 0) == 0) return false;
    if (line.rfind("internal ", 0) == 0) return false;
    if (line.rfind("external ", 0) == 0) return false;
    if (line.rfind("constant ", 0) == 0) return false;
    if (line.rfind("global ", 0) == 0) return false;

    // Label line: ends with ':'
    size_t lastNonSpace = line.find_last_not_of(" \t\r\n");
    if (lastNonSpace != std::string::npos && line[lastNonSpace] == ':') {
        // But "define i32 @foo:" is not a label in function body context
        // Only count labels that are function body labels (indented)
        if (c != ' ' && c != '\t') return false;
        // Indented label like "entry:" or "loop.header:" — this IS a label
        // but we count it separately in countBasicBlocks
        return false;
    }

    // If we got here and line contains '=', it's likely an instruction
    // Also check for common instruction keywords at the start
    if (line.find('=') != std::string::npos) return true;

    // Terminal instructions that don't have =
    if (line.rfind("ret ", 0) == 0) return true;
    if (line.rfind("br ", 0) == 0) return true;
    if (line.rfind("br\t", 0) == 0) return true;
    if (line.rfind("switch ", 0) == 0) return true;
    if (line.rfind("unreachable", 0) == 0) return true;
    if (line.rfind("store ", 0) == 0) return true;
    if (line.rfind("call ", 0) == 0) return true;
    if (line.rfind("invoke ", 0) == 0) return true;

    return false;
}

int IRAnalyzer::countInstructions(const std::string& ir) const {
    std::istringstream stream(ir);
    std::string line;
    int count = 0;
    bool inFunction = false;
    int braceDepth = 0;

    while (std::getline(stream, line)) {
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        std::string trimmed = line.substr(start);

        // Track function body boundaries
        if (trimmed.rfind("define ", 0) == 0) {
            inFunction = true;
            braceDepth = 0;
            // Count opening brace in define line
            for (char c : trimmed) {
                if (c == '{') braceDepth++;
                if (c == '}') braceDepth--;
            }
            continue;
        }

        if (!inFunction) continue;

        for (char c : trimmed) {
            if (c == '{') braceDepth++;
            if (c == '}') braceDepth--;
        }

        if (braceDepth <= 0) {
            inFunction = false;
            continue;
        }

        if (isInstructionLine(trimmed)) {
            count++;
        }
    }

    return count;
}

int IRAnalyzer::countBasicBlocks(const std::string& ir) const {
    std::istringstream stream(ir);
    std::string line;
    int totalBlocks = 0;
    bool inFunction = false;
    int braceDepth = 0;
    int labelCount = 0;
    bool hasInstrBeforeFirstLabel = false;
    bool seenLabel = false;

    while (std::getline(stream, line)) {
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        std::string trimmed = line.substr(start);

        if (trimmed.rfind("define ", 0) == 0) {
            if (inFunction) {
                totalBlocks += (labelCount > 0)
                                   ? labelCount
                                   : (hasInstrBeforeFirstLabel ? 1 : 0);
            }
            inFunction = true;
            braceDepth = 0;
            labelCount = 0;
            hasInstrBeforeFirstLabel = false;
            seenLabel = false;
            for (char c : trimmed) {
                if (c == '{') braceDepth++;
                if (c == '}') braceDepth--;
            }
            continue;
        }

        if (!inFunction) continue;

        for (char c : trimmed) {
            if (c == '{') braceDepth++;
            if (c == '}') braceDepth--;
        }

        if (braceDepth <= 0) {
            totalBlocks += (labelCount > 0) ? labelCount : 1;
            inFunction = false;
            continue;
        }

        if (trimmed[0] == ';') continue;

        size_t colonPos = trimmed.find(':');
        if (colonPos != std::string::npos) {
            std::string beforeColon = trimmed.substr(0, colonPos);
            bool isLabel = true;
            for (char c : beforeColon) {
                if (!isalnum(static_cast<unsigned char>(c)) && c != '.' &&
                    c != '_' && c != '-') {
                    isLabel = false;
                    break;
                }
            }
            if (isLabel && !beforeColon.empty()) {
                labelCount++;
                seenLabel = true;
                continue;
            }
        }

        if (!seenLabel && isInstructionLine(trimmed)) {
            hasInstrBeforeFirstLabel = true;
        }
    }

    if (inFunction) {
        totalBlocks += (labelCount > 0) ? labelCount : 1;
    }

    return totalBlocks;
}

int IRAnalyzer::countMemoryOps(const std::string& ir) const {
    return countOccurrences(ir, " load ") + countOccurrences(ir, " store ") +
           countOccurrences(ir, " alloca ") +
           countOccurrences(ir, " getelementptr ") +
           countOccurrences(ir, " memcpy") + countOccurrences(ir, " memset") +
           countOccurrences(ir, " memmove");
}

int IRAnalyzer::countBranchOps(const std::string& ir) const {
    return countOccurrences(ir, " br ") + countOccurrences(ir, " switch ");
}

int IRAnalyzer::countCallOps(const std::string& ir) const {
    return countOccurrences(ir, " call ");
}

int IRAnalyzer::countArithmeticOps(const std::string& ir) const {
    return countOccurrences(ir, " add ") + countOccurrences(ir, " sub ") +
           countOccurrences(ir, " mul ") + countOccurrences(ir, " sdiv ") +
           countOccurrences(ir, " udiv ") + countOccurrences(ir, " srem ") +
           countOccurrences(ir, " urem ") + countOccurrences(ir, " shl ") +
           countOccurrences(ir, " lshr ") + countOccurrences(ir, " ashr ") +
           countOccurrences(ir, " and ") + countOccurrences(ir, " or ") +
           countOccurrences(ir, " xor ");
}

}  // namespace phaseordering
