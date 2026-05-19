#include <gtest/gtest.h>

#include "cost_models.hpp"
#include "ir_analyzer.hpp"

using namespace phaseordering;

namespace {

const std::string kSimpleIR = R"(; ModuleID = 'test.c'
source_filename = "test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@.str = private unnamed_addr constant [13 x i8] c"hello world\0A\00", align 1

define i32 @main() {
entry:
  %call = call i32 @printf(ptr noundef @.str)
  ret i32 0
}

declare i32 @printf(ptr noundef, ...)
)";

const std::string kMultiFunctionIR = R"(source_filename = "math.c"
target triple = "x86_64-pc-linux-gnu"

define i32 @add(i32 %a, i32 %b) {
entry:
  %result = add i32 %a, %b
  ret i32 %result
}

define i32 @multiply(i32 %x, i32 %y) {
entry:
  %result = mul i32 %x, %y
  ret i32 %result
}

define i32 @compute(i32 %n) {
entry:
  %cmp = icmp sgt i32 %n, 0
  br i1 %cmp, label %if.then, label %if.end

if.then:
  %sum = add i32 %n, 10
  %prod = mul i32 %sum, 2
  br label %if.end

if.end:
  %phi = phi i32 [ %sum, %if.then ], [ %n, %entry ]
  %stored = alloca i32
  store i32 %phi, ptr %stored
  %loaded = load i32, ptr %stored
  %final = add i32 %loaded, 1
  ret i32 %final
}
)";

const std::string kLoopIR = R"(source_filename = "loop.c"
target triple = "x86_64-pc-linux-gnu"

define i32 @sum_to_n(i32 %n) {
entry:
  %cmp = icmp sgt i32 %n, 0
  br i1 %cmp, label %loop.header, label %exit

loop.header:
  %i = phi i32 [ 0, %entry ], [ %next, %loop.body ]
  %total = phi i32 [ 0, %entry ], [ %new_total, %loop.body ]
  %cond = icmp slt i32 %i, %n
  br i1 %cond, label %loop.body, label %exit

loop.body:
  %new_total = add i32 %total, %i
  %next = add i32 %i, 1
  br label %loop.header

exit:
  %result = phi i32 [ 0, %entry ], [ %total, %loop.header ]
  ret i32 %result
}
)";

const std::string kRealisticIR = R"(; ModuleID = 'test.cpp'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@global_var = dso_local global i32 42, align 4
@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

define dso_local i32 @fibonacci(i32 %n) {
entry:
  %cmp = icmp slt i32 %n, 2
  br i1 %cmp, label %return, label %if.end

if.end:
  %n1 = sub i32 %n, 1
  %n2 = sub i32 %n, 2
  %call1 = call i32 @fibonacci(i32 %n1)
  %call2 = call i32 @fibonacci(i32 %n2)
  %add = add i32 %call1, %call2
  br label %return

return:
  %result = phi i32 [ %n, %entry ], [ %add, %if.end ]
  ret i32 %result
}

define dso_local i32 @main() {
entry:
  %val = load i32, ptr @global_var
  %result = call i32 @fibonacci(i32 %val)
  %fmt = call i32 @printf(ptr @.str, i32 %result)
  ret i32 0
}

declare i32 @printf(ptr, ...)
)";

}  // namespace

TEST(IRAnalyzerTest, SimpleIR_FunctionCount) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kSimpleIR);
    EXPECT_EQ(metrics.totalFunctions, 1);
}

TEST(IRAnalyzerTest, SimpleIR_InstructionCount) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kSimpleIR);
    EXPECT_EQ(metrics.totalInstructions, 2);
}

TEST(IRAnalyzerTest, SimpleIR_BasicBlocks) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kSimpleIR);
    EXPECT_EQ(metrics.totalBasicBlocks, 1);
}

TEST(IRAnalyzerTest, SimpleIR_CallOps) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kSimpleIR);
    EXPECT_EQ(metrics.callOps, 1);
}

TEST(IRAnalyzerTest, SimpleIR_MemoryOps) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kSimpleIR);
    EXPECT_EQ(metrics.memoryOps, 0);
}

TEST(IRAnalyzerTest, SimpleIR_BranchOps) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kSimpleIR);
    EXPECT_EQ(metrics.branchOps, 0);
}

TEST(IRAnalyzerTest, SimpleIR_ArithmeticOps) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kSimpleIR);
    EXPECT_EQ(metrics.arithmeticOps, 0);
}

TEST(IRAnalyzerTest, MultiFunction_FunctionCount) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kMultiFunctionIR);
    EXPECT_EQ(metrics.totalFunctions, 3);
}

TEST(IRAnalyzerTest, MultiFunction_InstructionCount) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kMultiFunctionIR);
    EXPECT_EQ(metrics.totalInstructions, 15);
}

TEST(IRAnalyzerTest, MultiFunction_BasicBlocks) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kMultiFunctionIR);
    EXPECT_EQ(metrics.totalBasicBlocks, 5);
}

TEST(IRAnalyzerTest, MultiFunction_MemoryOps) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kMultiFunctionIR);
    EXPECT_EQ(metrics.memoryOps, 3);
}

TEST(IRAnalyzerTest, MultiFunction_CallOps) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kMultiFunctionIR);
    EXPECT_EQ(metrics.callOps, 0);
}

TEST(IRAnalyzerTest, MultiFunction_ArithmeticOps) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kMultiFunctionIR);
    EXPECT_EQ(metrics.arithmeticOps, 5);
}

TEST(IRAnalyzerTest, MultiFunction_BranchOps) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kMultiFunctionIR);
    EXPECT_EQ(metrics.branchOps, 2);
}

TEST(IRAnalyzerTest, LoopIR_FunctionCount) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kLoopIR);
    EXPECT_EQ(metrics.totalFunctions, 1);
}

TEST(IRAnalyzerTest, LoopIR_InstructionCount) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kLoopIR);
    EXPECT_EQ(metrics.totalInstructions, 11);
}

TEST(IRAnalyzerTest, LoopIR_BasicBlocks) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kLoopIR);
    EXPECT_EQ(metrics.totalBasicBlocks, 4);
}

TEST(IRAnalyzerTest, LoopIR_BranchOps) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kLoopIR);
    EXPECT_EQ(metrics.branchOps, 3);
}

TEST(IRAnalyzerTest, LoopIR_ArithmeticOps) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kLoopIR);
    EXPECT_EQ(metrics.arithmeticOps, 2);
}

TEST(IRAnalyzerTest, RealisticIR_FunctionCount) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kRealisticIR);
    EXPECT_EQ(metrics.totalFunctions, 2);
}

TEST(IRAnalyzerTest, RealisticIR_InstructionCount) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kRealisticIR);
    EXPECT_EQ(metrics.totalInstructions, 14);
}

TEST(IRAnalyzerTest, RealisticIR_BasicBlocks) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kRealisticIR);
    EXPECT_EQ(metrics.totalBasicBlocks, 4);
}

TEST(IRAnalyzerTest, RealisticIR_CallOps) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kRealisticIR);
    EXPECT_EQ(metrics.callOps, 4);
}

TEST(IRAnalyzerTest, RealisticIR_MemoryOps) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(kRealisticIR);
    EXPECT_GE(metrics.memoryOps, 1);
}

TEST(IRAnalyzerTest, EmptyIR_ReturnsInvalid) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze("");
    EXPECT_EQ(metrics.totalFunctions, 0);
    EXPECT_LT(metrics.totalInstructions, 0);
}

TEST(IRAnalyzerTest, ShortIR_ReturnsInvalid) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze("short");
    EXPECT_LT(metrics.totalInstructions, 0);
}

TEST(IRAnalyzerTest, DeclarationOnly_NoFunctions) {
    IRAnalyzer analyzer;
    auto metrics = analyzer.analyze(
        "declare i32 @printf(ptr, ...)\ndeclare void @exit(i32)\n");
    EXPECT_EQ(metrics.totalFunctions, 0);
    EXPECT_LT(metrics.totalInstructions, 0);
}

TEST(IRAnalyzerTest, NoDefineKeyword_ReturnsInvalid) {
    IRAnalyzer analyzer;
    auto metrics =
        analyzer.analyze("this is some random text without any functions");
    EXPECT_LT(metrics.totalInstructions, 0);
}

TEST(IRAnalyzerTest, InvalidMetricsReturns1e9InCostModel) {
    InstructionCountCostModel model;
    IRMetrics invalid;
    invalid.totalInstructions = -1;
    IRMetrics baseline;
    baseline.totalInstructions = 100;
    double score = model.score(invalid, baseline);
    EXPECT_GE(score, 1e8);
}
