#include "ir_analyzer.hpp"

#include <sstream>
#include <string>

namespace phaseordering {

namespace {

bool isInstructionLine(const std::string& line) {
    if (line.empty()) return false;

    char c = line[0];

    if (c == ';' || c == '!' || c == '{' || c == '}') return false;

    if (line.rfind("define ", 0) == 0) return false;
    if (line.rfind("declare ", 0) == 0) return false;
    if (line.rfind("source_filename", 0) == 0) return false;
    if (line.rfind("target ", 0) == 0) return false;
    if (line.rfind("attributes ", 0) == 0) return false;
    if (line.rfind("label ", 0) == 0) return false;
    if (line.rfind("unique ", 0) == 0) return false;
    if (line.rfind("dso_local", 0) == 0) return false;
    if (line.rfind("private ", 0) == 0) return false;
    if (line.rfind("internal ", 0) == 0) return false;
    if (line.rfind("external ", 0) == 0) return false;
    if (line.rfind("constant ", 0) == 0) return false;
    if (line.rfind("global ", 0) == 0) return false;

    size_t lastNonSpace = line.find_last_not_of(" \t\r\n");
    if (lastNonSpace != std::string::npos && line[lastNonSpace] == ':') {
        return false;
    }

    if (line.find('=') != std::string::npos) return true;

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

std::string extractInstructionText(const std::string& ir) {
    std::istringstream stream(ir);
    std::string line;
    std::string result;
    bool inFunction = false;
    int braceDepth = 0;

    while (std::getline(stream, line)) {
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        std::string trimmed = line.substr(start);

        if (trimmed.rfind("define ", 0) == 0) {
            inFunction = true;
            braceDepth = 0;
            for (char ch : trimmed) {
                if (ch == '{') braceDepth++;
                if (ch == '}') braceDepth--;
            }
            continue;
        }

        if (!inFunction) continue;

        for (char ch : trimmed) {
            if (ch == '{') braceDepth++;
            if (ch == '}') braceDepth--;
        }

        if (braceDepth <= 0) {
            inFunction = false;
            continue;
        }

        if (isInstructionLine(trimmed)) {
            result += trimmed;
            result += ' ';
        }
    }

    return result;
}

}  // anonymous namespace

IRMetrics IRAnalyzer::analyze(const std::string& ir) const {
    IRMetrics metrics;

    if (ir.empty()) {
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

    std::string instrText = extractInstructionText(ir);
    std::string paddedInstrText = " " + instrText + " ";
    metrics.memoryOps = countMemoryOps(paddedInstrText);
    metrics.branchOps = countBranchOps(paddedInstrText);
    metrics.callOps = countCallOps(paddedInstrText);
    metrics.arithmeticOps = countArithmeticOps(paddedInstrText);

    metrics.avgBasicBlockSize =
        metrics.totalBasicBlocks > 0
            ? static_cast<double>(metrics.totalInstructions) /
                  metrics.totalBasicBlocks
            : 0.0;
    metrics.maxBlockDepth = 0;
    metrics.executionTimeMs = 0.0;

    if (metrics.totalInstructions == 0) {
        metrics.totalInstructions = -1;
    }

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
        size_t brace = ir.find('{', pos);
        if (brace == std::string::npos) break;
        count++;
        pos = brace + 1;
    }
    return count;
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

        if (trimmed.rfind("define ", 0) == 0) {
            inFunction = true;
            braceDepth = 0;
            for (char ch : trimmed) {
                if (ch == '{') braceDepth++;
                if (ch == '}') braceDepth--;
            }
            continue;
        }

        if (!inFunction) continue;

        for (char ch : trimmed) {
            if (ch == '{') braceDepth++;
            if (ch == '}') braceDepth--;
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
            for (char ch : trimmed) {
                if (ch == '{') braceDepth++;
                if (ch == '}') braceDepth--;
            }
            continue;
        }

        if (!inFunction) continue;

        for (char ch : trimmed) {
            if (ch == '{') braceDepth++;
            if (ch == '}') braceDepth--;
        }

        if (braceDepth <= 0) {
            totalBlocks += (labelCount > 0)
                               ? labelCount
                               : (hasInstrBeforeFirstLabel ? 1 : 0);
            inFunction = false;
            continue;
        }

        if (trimmed[0] == ';') continue;

        size_t colonPos = trimmed.find(':');
        if (colonPos != std::string::npos) {
            std::string beforeColon = trimmed.substr(0, colonPos);
            bool isLabel = !beforeColon.empty();
            for (char ch : beforeColon) {
                if (!isalnum(static_cast<unsigned char>(ch)) && ch != '.' &&
                    ch != '_' && ch != '-') {
                    isLabel = false;
                    break;
                }
            }
            if (isLabel) {
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
        totalBlocks +=
            (labelCount > 0) ? labelCount : (hasInstrBeforeFirstLabel ? 1 : 0);
    }

    return totalBlocks;
}

int IRAnalyzer::countMemoryOps(const std::string& instrText) const {
    return countOccurrences(instrText, " load ") +
           countOccurrences(instrText, " store ") +
           countOccurrences(instrText, " alloca ") +
           countOccurrences(instrText, " getelementptr ") +
           countOccurrences(instrText, " memcpy") +
           countOccurrences(instrText, " memset") +
           countOccurrences(instrText, " memmove");
}

int IRAnalyzer::countBranchOps(const std::string& instrText) const {
    return countOccurrences(instrText, " br ") +
           countOccurrences(instrText, " switch ");
}

int IRAnalyzer::countCallOps(const std::string& instrText) const {
    return countOccurrences(instrText, " call ") +
           countOccurrences(instrText, " invoke ");
}

int IRAnalyzer::countArithmeticOps(const std::string& instrText) const {
    return countOccurrences(instrText, " add ") +
           countOccurrences(instrText, " sub ") +
           countOccurrences(instrText, " mul ") +
           countOccurrences(instrText, " sdiv ") +
           countOccurrences(instrText, " udiv ") +
           countOccurrences(instrText, " srem ") +
           countOccurrences(instrText, " urem ") +
           countOccurrences(instrText, " shl ") +
           countOccurrences(instrText, " lshr ") +
           countOccurrences(instrText, " ashr ") +
           countOccurrences(instrText, " and ") +
           countOccurrences(instrText, " or ") +
           countOccurrences(instrText, " xor ");
}

}  // namespace phaseordering
