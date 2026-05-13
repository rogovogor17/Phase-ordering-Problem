#include "llvm_facade.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#ifndef _WIN32
    #include <sys/times.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

namespace phaseordering {

namespace fs = std::filesystem;

void LLVMConfig::resolvePaths() {
    if (!llvmBinPath.empty()) {
        fs::path binDir(llvmBinPath);

#ifdef _WIN32
        if (clangPath.empty()) clangPath = (binDir / "clang.exe").string();
        if (optPath.empty()) optPath = (binDir / "opt.exe").string();
        if (lliPath.empty()) lliPath = (binDir / "lli.exe").string();
#else
        if (clangPath.empty()) clangPath = (binDir / "clang").string();
        if (optPath.empty()) optPath = (binDir / "opt").string();
        if (lliPath.empty()) lliPath = (binDir / "lli").string();
#endif
    }
}

static std::vector<std::string> getVersionedNames(const std::string& baseTool) {
    std::vector<std::string> names;
    names.push_back(baseTool);
    for (int v = 20; v >= 14; --v) {
        names.push_back(baseTool + "-" + std::to_string(v));
    }
    return names;
}

LLVMFacade::LLVMFacade(const LLVMConfig& config) : config_(config) {
    config_.resolvePaths();

    if (config_.clangPath.empty()) {
        for (const auto& name : getVersionedNames("clang")) {
            std::string found = findTool(name);
            if (!found.empty()) {
                config_.clangPath = found;
                break;
            }
        }
    }
    if (config_.optPath.empty()) {
        for (const auto& name : getVersionedNames("opt")) {
            std::string found = findTool(name);
            if (!found.empty()) {
                config_.optPath = found;
                break;
            }
        }
    }
    if (config_.lliPath.empty()) {
        for (const auto& name : getVersionedNames("lli")) {
            std::string found = findTool(name);
            if (!found.empty()) {
                config_.lliPath = found;
                break;
            }
        }
    }

    available_ = !config_.clangPath.empty() && !config_.optPath.empty();
}

std::string LLVMFacade::compileToIR(const std::string& sourceFile) const {
    lastError_.clear();

    if (config_.clangPath.empty()) {
        lastError_ = "clang not found. Install LLVM/Clang or use --llvm-path";
        return "";
    }

    std::string tempOutput = writeTempFile("", ".ll");

    std::ostringstream cmd;
    cmd << "\"" << config_.clangPath << "\" -S -emit-llvm -O0 \"" << sourceFile
        << "\" -o \"" << tempOutput << "\" 2>&1";

    int exitCode = 0;
    std::string output = executeCommand(cmd.str(), &exitCode);

    if (exitCode != 0) {
        lastError_ = "clang failed (exit code " + std::to_string(exitCode) +
                     "): " + output;
        removeTempFile(tempOutput);
        return "";
    }

    std::ifstream file(tempOutput);
    if (!file.is_open()) {
        lastError_ = "Failed to open output IR file: " + tempOutput;
        removeTempFile(tempOutput);
        return "";
    }
    std::string ir((std::istreambuf_iterator<char>(file)),
                   std::istreambuf_iterator<char>());
    file.close();
    removeTempFile(tempOutput);

    if (ir.empty()) {
        lastError_ = "Compiled IR is empty";
    }

    return ir;
}

std::string LLVMFacade::applyPasses(const std::string& ir,
                                    const OptSequence& seq) const {
    lastError_.clear();

    if (config_.optPath.empty()) {
        lastError_ = "opt not found. Install LLVM or use --llvm-path";
        return "";
    }

    if (seq.empty()) {
        return ir;
    }

    std::string passList;
    const auto& passes = seq.passes();

    for (size_t i = 0; i < passes.size(); ++i) {
        if (!passList.empty()) passList += ",";
        passList += passes[i];
    }

    // Hard cap: prevent absurdly long pass lists that would cause timeout
    static const int MAX_PASSES = 30;
    int count = 0;
    std::string cappedList;
    for (size_t i = 0; i < passList.size();) {
        size_t comma = passList.find(',', i);
        std::string token = (comma == std::string::npos)
                                ? passList.substr(i)
                                : passList.substr(i, comma - i);
        if (!cappedList.empty()) cappedList += ",";
        cappedList += token;
        count++;
        if (count >= MAX_PASSES) break;
        if (comma == std::string::npos) break;
        i = comma + 1;
    }
    passList = cappedList;

    // If all passes were duplicates of an empty list, return original
    if (passList.empty()) {
        return ir;
    }

    std::string tempInput = writeTempFile(ir, ".ll");
    std::string tempOutput = writeTempFile("", ".ll");

    std::ostringstream cmd;
#ifdef _WIN32
    cmd << "\"" << config_.optPath << "\" -passes=\"" << passList << "\" \""
        << tempInput << "\" -S -o \"" << tempOutput << "\" 2>&1";
#else
    cmd << "timeout " << (config_.timeoutMs / 1000) << " \"" << config_.optPath
        << "\" -passes=\"" << passList << "\" \"" << tempInput << "\" -S -o \""
        << tempOutput << "\" 2>&1";
#endif

    int exitCode = 0;
    std::string output = executeCommand(cmd.str(), &exitCode);

    if (exitCode != 0) {
        lastError_ = "opt failed (exit code " + std::to_string(exitCode) +
                     "): " + output;
        removeTempFile(tempInput);
        removeTempFile(tempOutput);
        return "";
    }

    std::ifstream file(tempOutput);
    if (!file.is_open()) {
        lastError_ = "Failed to open optimized IR file: " + tempOutput;
        removeTempFile(tempInput);
        removeTempFile(tempOutput);
        return "";
    }
    std::string result((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();

    removeTempFile(tempInput);
    removeTempFile(tempOutput);

    return result;
}

std::string LLVMFacade::compileToNative(const std::string& ir) const {
    lastError_.clear();

    if (config_.clangPath.empty()) {
        lastError_ = "clang not found. Install LLVM/Clang or use --llvm-path";
        return "";
    }

    std::string tempIR = writeTempFile(ir, ".ll");
#ifdef _WIN32
    std::string tempExe = writeTempFile("", ".exe");
#else
    std::string tempExe = writeTempFile("", ".out");
#endif

    std::ostringstream cmd;
    cmd << "\"" << config_.clangPath << "\" \"" << tempIR << "\" -o \""
        << tempExe << "\" 2>&1";

    int exitCode = 0;
    std::string output = executeCommand(cmd.str(), &exitCode);

    removeTempFile(tempIR);

    if (exitCode != 0) {
        lastError_ = "clang native compilation failed: " + output;
        removeTempFile(tempExe);
        return "";
    }

    return tempExe;
}

double LLVMFacade::runInterpreter(const std::string& ir) const {
    lastError_.clear();

    if (config_.lliPath.empty()) {
        lastError_ = "lli not found. Install LLVM or use --llvm-path";
        return -1.0;
    }

    std::string tempIR = writeTempFile(ir, ".ll");

    std::ostringstream cmd;
    cmd << "\"" << config_.lliPath << "\" \"" << tempIR << "\" 2>&1";

#ifndef _WIN32
    struct tms startTms;
    struct tms endTms;
    times(&startTms);
#endif
    auto startWall = std::chrono::high_resolution_clock::now();

    int exitCode = 0;
    executeCommand(cmd.str(), &exitCode);

    auto endWall = std::chrono::high_resolution_clock::now();
#ifndef _WIN32
    times(&endTms);
    long clk = sysconf(_SC_CLK_TCK);
    double elapsedMs = (endTms.tms_cutime - startTms.tms_cutime +
                        endTms.tms_cstime - startTms.tms_cstime) *
                       1000.0 / clk;
#else
    double elapsedMs =
        std::chrono::duration<double, std::milli>(endWall - startWall).count();
#endif

    removeTempFile(tempIR);

    if (exitCode != 0) {
        lastError_ = "lli failed (exit code " + std::to_string(exitCode) + ")";
        return -1.0;
    }
    return elapsedMs;
}

double LLVMFacade::runNative(const std::string& executablePath) const {
#ifndef _WIN32
    struct tms startTms;
    struct tms endTms;
    times(&startTms);
#endif
    auto startWall = std::chrono::high_resolution_clock::now();

    std::ostringstream cmd;
    cmd << "\"" << executablePath << "\" 2>&1";

    int exitCode = 0;
    executeCommand(cmd.str(), &exitCode);

    auto endWall = std::chrono::high_resolution_clock::now();
#ifndef _WIN32
    times(&endTms);
    long clk = sysconf(_SC_CLK_TCK);
    double elapsedMs = (endTms.tms_cutime - startTms.tms_cutime +
                        endTms.tms_cstime - startTms.tms_cstime) *
                       1000.0 / clk;
#else
    double elapsedMs =
        std::chrono::duration<double, std::milli>(endWall - startWall).count();
#endif

    if (exitCode != 0) {
        lastError_ = "Native execution failed (exit code " +
                     std::to_string(exitCode) + ")";
        return -1.0;
    }
    return elapsedMs;
}

bool LLVMFacade::verifyIR(const std::string& ir) const {
    lastError_.clear();

    if (config_.optPath.empty()) {
        lastError_ = "opt not found";
        return false;
    }

    std::string tempIR = writeTempFile(ir, ".ll");
    std::string tempOut = writeTempFile("", ".ll");

    std::ostringstream cmd;
    cmd << "\"" << config_.optPath << "\" -passes=verify \"" << tempIR
        << "\" -S -o \"" << tempOut << "\" 2>&1";

    int exitCode = 0;
    executeCommand(cmd.str(), &exitCode);

    removeTempFile(tempIR);
    removeTempFile(tempOut);
    return exitCode == 0;
}

bool LLVMFacade::isAvailable() const { return available_; }

std::string LLVMFacade::getLastError() const { return lastError_; }

std::string LLVMFacade::getClangPath() const { return config_.clangPath; }

std::string LLVMFacade::getOptPath() const { return config_.optPath; }

std::string LLVMFacade::getLliPath() const { return config_.lliPath; }

int decodeExitCode(int rawCode) {
#ifdef _WIN32
    return rawCode;
#else
    if (WIFEXITED(rawCode)) {
        return WEXITSTATUS(rawCode);
    }
    return -1;
#endif
}

std::string LLVMFacade::executeCommand(const std::string& command,
                                       int* exitCode) const {
    std::array<char, 8192> buffer;
    std::string result;

#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif

    if (!pipe) {
        if (exitCode) *exitCode = -1;
        lastError_ = "Failed to execute command: " + command;
        return "";
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) !=
           nullptr) {
        result += buffer.data();
    }

#ifdef _WIN32
    int rawCode = _pclose(pipe);
#else
    int rawCode = pclose(pipe);
#endif

    int code = decodeExitCode(rawCode);
    if (exitCode) *exitCode = code;
    return result;
}

std::string LLVMFacade::writeTempFile(const std::string& content,
                                      const std::string& suffix) const {
    static std::atomic<int> counter{0};
    std::string tempPath = std::filesystem::temp_directory_path().string()
#ifdef _WIN32
                           + "\\phaseordering_"
#else
                           + "/phaseordering_"
#endif
                           + std::to_string(++counter) + suffix;

    if (!content.empty()) {
        std::ofstream file(tempPath);
        file << content;
        file.close();
    }
    return tempPath;
}

void LLVMFacade::removeTempFile(const std::string& path) const {
    if (!path.empty()) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
}

std::string LLVMFacade::findTool(const std::string& toolName) const {
    std::string cmd =
#ifdef _WIN32
        "where " + toolName + " 2>&1";
#else
        "which " + toolName + " 2>/dev/null";
#endif
    int exitCode = 0;
    std::string result = executeCommand(cmd, &exitCode);
    if (exitCode == 0 && !result.empty()) {
        size_t nl = result.find_first_of("\r\n");
        if (nl != std::string::npos) {
            result = result.substr(0, nl);
        }
        while (!result.empty() &&
               (result.back() == ' ' || result.back() == '\t')) {
            result.pop_back();
        }
        if (!result.empty()) {
            return result;
        }
    }
    return "";
}

}  // namespace phaseordering
