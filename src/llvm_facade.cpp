#include "llvm_facade.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#ifndef _WIN32
    #include <sys/times.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

#ifdef __APPLE__
    #include <AvailabilityMacros.h>
#endif

namespace phaseordering {

namespace fs = std::filesystem;

void LLVMConfig::resolvePaths() {
    if (!llvmBinPath.empty()) {
        std::string dirStr = llvmBinPath;

        // Remove trailing slashes so path concatenation doesn't produce
        // double slashes (e.g. "/path//clang").
        while (!dirStr.empty() &&
               (dirStr.back() == '/' || dirStr.back() == '\\')) {
            dirStr.pop_back();
        }

        // If the path looks like a binary (contains an executable name),
        // take the parent directory. We check common binary patterns:
        // ends with "clang", "clang-*", "opt", "opt-*", "lli", "lli-*"
        // or contains a dot (Windows .exe/.bat etc).
        fs::path p(dirStr);
        std::string filename = p.filename().string();
        bool looksLikeBinary = false;

        if (filename.find('.') != std::string::npos) {
            // Has an extension (like .exe) — likely a binary
            looksLikeBinary = true;
        } else {
            // Check if filename matches known tool names (possibly versioned)
            static const std::vector<std::string> toolPrefixes = {
                "clang", "opt", "lli", "llvm-link"};
            for (const auto& prefix : toolPrefixes) {
                if (filename == prefix ||
                    filename.rfind(prefix + "-", 0) == 0) {
                    looksLikeBinary = true;
                    break;
                }
            }
        }

        if (looksLikeBinary) {
            dirStr = p.parent_path().string();
        }

        fs::path binDir(dirStr);

#ifdef _WIN32
        if (clangPath.empty()) clangPath = (binDir / "clang.exe").string();
        if (optPath.empty()) optPath = (binDir / "opt.exe").string();
        if (lliPath.empty()) lliPath = (binDir / "lli.exe").string();
        if (llvmLinkPath.empty())
            llvmLinkPath = (binDir / "llvm-link.exe").string();
#else
        if (clangPath.empty()) clangPath = (binDir / "clang").string();
        if (optPath.empty()) optPath = (binDir / "opt").string();
        if (lliPath.empty()) lliPath = (binDir / "lli").string();
        if (llvmLinkPath.empty())
            llvmLinkPath = (binDir / "llvm-link").string();
#endif
    }
}

static std::vector<std::string> getVersionedNames(const std::string& baseTool) {
    std::vector<std::string> names;
    names.push_back(baseTool);
    for (int v = 25; v >= 14; --v) {
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
    if (config_.llvmLinkPath.empty()) {
        for (const auto& name : getVersionedNames("llvm-link")) {
            std::string found = findTool(name);
            if (!found.empty()) {
                config_.llvmLinkPath = found;
                break;
            }
        }
    }

    available_ = !config_.clangPath.empty() && !config_.optPath.empty();

    // Detect timeout command availability.
    // On Linux: "timeout" (coreutils)
    // On macOS: "gtimeout" (GNU coreutils via Homebrew) or nothing
    // On Windows: not used (we skip timeout)
#ifdef _WIN32
    timeoutCmd_ = "";
#else
    timeoutCmd_ = findTool("timeout");
    if (timeoutCmd_.empty()) {
        // On macOS with GNU coreutils installed via Homebrew
        timeoutCmd_ = findTool("gtimeout");
    }
#endif
}

std::string LLVMFacade::buildTimeoutPrefix() const {
    if (timeoutCmd_.empty()) return "";

    double seconds = static_cast<double>(config_.timeoutMs) / 1000.0;
    if (seconds < 1.0) seconds = 1.0;

    std::ostringstream oss;
    oss << timeoutCmd_ << " " << std::fixed << std::setprecision(1) << seconds
        << " ";
    return oss.str();
}

std::string LLVMFacade::compileToIR(const std::string& sourceFile) const {
    lastError_.clear();

    if (config_.clangPath.empty()) {
        lastError_ = "clang not found. Install LLVM/Clang or use --llvm-path";
        return "";
    }

    std::string tempOutput = writeTempFile("", ".ll");

    std::ostringstream cmd;
    cmd << buildTimeoutPrefix() << "\"" << config_.clangPath
        << "\" -S -emit-llvm -O0";
    if (!config_.extraClangFlags.empty()) {
        cmd << " " << config_.extraClangFlags;
    }
    cmd << " \"" << sourceFile << "\" -o \"" << tempOutput << "\" 2>&1";

    std::string cmdStr = cmd.str();
    int exitCode = 0;
    std::string output = executeCommand(cmdStr, &exitCode);

    if (exitCode != 0) {
        lastError_ = "clang failed (exit code " + std::to_string(exitCode) +
                     "): " + output + "  CMD: " + cmdStr;
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
        return "";
    }

    if (ir.find("define ") == std::string::npos) {
        lastError_ =
            "Compiled IR contains no function definitions (empty or "
            "header-only source?)";
        return "";
    }

    return ir;
}

std::string LLVMFacade::linkModules(
    const std::vector<std::string>& irModules) const {
    lastError_.clear();

    if (irModules.size() <= 1) {
        return irModules.empty() ? "" : irModules[0];
    }

    if (config_.llvmLinkPath.empty()) {
        lastError_ = "llvm-link not found. Install LLVM or use --llvm-path";
        return "";
    }

    std::vector<std::string> tempFiles;
    std::string tempOutput = writeTempFile("", ".ll");

    for (size_t i = 0; i < irModules.size(); ++i) {
        std::string tmp = writeTempFile(irModules[i], ".ll");
        tempFiles.push_back(tmp);
    }

    std::ostringstream cmd;
    cmd << buildTimeoutPrefix() << "\"" << config_.llvmLinkPath << "\" -S";
    for (const auto& tf : tempFiles) {
        cmd << " \"" << tf << "\"";
    }
    cmd << " -o \"" << tempOutput << "\" 2>&1";

    int exitCode = 0;
    std::string output = executeCommand(cmd.str(), &exitCode);

    for (const auto& tf : tempFiles) removeTempFile(tf);

    if (exitCode != 0) {
        lastError_ = "llvm-link failed (exit code " + std::to_string(exitCode) +
                     "): " + output;
        removeTempFile(tempOutput);
        return "";
    }

    std::ifstream file(tempOutput);
    if (!file.is_open()) {
        lastError_ = "Failed to open linked IR file: " + tempOutput;
        removeTempFile(tempOutput);
        return "";
    }
    std::string linked((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();
    removeTempFile(tempOutput);

    if (linked.empty()) {
        lastError_ = "llvm-link produced empty output";
        return "";
    }

    return linked;
}

std::string LLVMFacade::applyPasses(const std::string& ir,
                                    const OptSequence& seq) const {
    lastError_.clear();

    if (config_.optPath.empty()) {
        lastError_ = "opt not found. Install LLVM or use --llvm-path";
        return "";
    }

    std::string passList;
    if (!seq.empty()) {
        const auto& passes = seq.passes();
        int count = 0;
        for (const auto& p : passes) {
            if (count >= kMaxSequenceLength) break;
            if (!passList.empty()) passList += ",";
            passList += p;
            ++count;
        }
    }

    std::string tempInput = writeTempFile(ir, ".ll");
    std::string tempOutput = writeTempFile("", ".ll");

    std::ostringstream cmd;
    std::string timeoutPrefix = buildTimeoutPrefix();

    if (passList.empty()) {
        cmd << timeoutPrefix << "\"" << config_.optPath << "\" -passes=\"\" \""
            << tempInput << "\" -S -o \"" << tempOutput << "\" 2>&1";
    } else {
        cmd << timeoutPrefix << "\"" << config_.optPath << "\" -passes=\""
            << passList << "\" \"" << tempInput << "\" -S -o \"" << tempOutput
            << "\" 2>&1";
    }

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

    if (result.empty()) {
        lastError_ =
            "opt produced empty output for pass list: [" + passList + "]";
        return "";
    }

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
    cmd << buildTimeoutPrefix() << "\"" << config_.clangPath << "\" \""
        << tempIR << "\" -o \"" << tempExe << "\" 2>&1";

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
    cmd << buildTimeoutPrefix() << "\"" << config_.lliPath << "\" \"" << tempIR
        << "\" 2>&1";

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
    if (elapsedMs <= 0.0) {
        elapsedMs =
            std::chrono::duration<double, std::milli>(endWall - startWall)
                .count();
    }
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
    cmd << buildTimeoutPrefix() << "\"" << executablePath << "\" 2>&1";

    int exitCode = 0;
    executeCommand(cmd.str(), &exitCode);

    auto endWall = std::chrono::high_resolution_clock::now();
#ifndef _WIN32
    times(&endTms);
    long clk = sysconf(_SC_CLK_TCK);
    double elapsedMs = (endTms.tms_cutime - startTms.tms_cutime +
                        endTms.tms_cstime - startTms.tms_cstime) *
                       1000.0 / clk;
    if (elapsedMs <= 0.0) {
        elapsedMs =
            std::chrono::duration<double, std::milli>(endWall - startWall)
                .count();
    }
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
    cmd << buildTimeoutPrefix() << "\"" << config_.optPath
        << "\" -passes=verify \"" << tempIR << "\" -S -o \"" << tempOut
        << "\" 2>&1";

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

namespace {

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

}  // anonymous namespace

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
    static std::atomic<int> counter{0};  // NOTE
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
