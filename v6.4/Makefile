# ============================================================
# myclang-c++ — 可移植 Makefile
#
# 系统安装 LLVM (apt/pacman):  make
# 自编译 LLVM:                 make LLVM_CONFIG=... LLVM_SRC=...
# ============================================================

ifndef VERBOSE
QUIET:=@
endif

SRC_DIR?=$(PWD)

# --- llvm-config 自动探测 ---
LLVM_CONFIG ?= $(shell \
    which llvm-config 2>/dev/null || \
    which llvm-config-22 2>/dev/null || \
    which llvm-config-21 2>/dev/null || \
    which llvm-config-20 2>/dev/null || \
    which llvm-config-19 2>/dev/null || \
    which llvm-config-18 2>/dev/null || \
    which llvm-config-17 2>/dev/null || \
    echo llvm-config)

# --- 编译选项 ---
CXXFLAGS += $(shell $(LLVM_CONFIG) --cxxflags)
CXXFLAGS += -fno-rtti

# --- 头文件路径 ---
ifdef LLVM_SRC
  # 自编译 LLVM：需要 clang 源码中的头文件 + 构建目录中的生成头文件
  CPPFLAGS += -I$(LLVM_SRC)/clang/include
  LLVM_BUILD_DIR ?= $(abspath $(dir $(LLVM_CONFIG))/..)
  CPPFLAGS += -I$(LLVM_BUILD_DIR)/tools/clang/include
  CPPFLAGS += -I$(LLVM_BUILD_DIR)/include
endif
CPPFLAGS += -I$(SRC_DIR)

# --- 链接 ---
LDFLAGS  += $(shell $(LLVM_CONFIG) --ldflags)
SYSTEMLIBS = $(shell $(LLVM_CONFIG) --system-libs) -l:libzstd.so.1 -l:libz.so.1 -ltinfo

ifdef LLVM_SRC
  # 自编译：静态链接，需完整依赖链
  LLVMLIBS = $(shell $(LLVM_CONFIG) --libs all)
  CLANGLIBS = \
	-lclangFrontend \
	-lclangFrontendTool \
	-lclangDriver \
	-lclangSerialization \
	-lclangCodeGen \
	-lclangParse \
	-lclangSema \
	-lclangAnalysis \
	-lclangAnalysisFlowSensitive \
	-lclangAnalysisFlowSensitiveModels \
	-lclangEdit \
	-lclangASTMatchers \
	-lclangDynamicASTMatchers \
	-lclangAST \
	-lclangLex \
	-lclangAPINotes \
	-lclangSupport \
	-lclangOptions \
	-lclangIndex \
	-lclangIndexSerialization \
	-lclangTooling \
	-lclangToolingCore \
	-lclangToolingASTDiff \
	-lclangToolingInclusions \
	-lclangToolingInclusionsStdlib \
	-lclangToolingRefactoring \
	-lclangToolingSyntax \
	-lclangRewrite \
	-lclangRewriteFrontend \
	-lclangStaticAnalyzerFrontend \
	-lclangStaticAnalyzerCheckers \
	-lclangStaticAnalyzerCore \
	-lclangCrossTU \
	-lclangDependencyScanning \
	-lclangDirectoryWatcher \
	-lclangExtractAPI \
	-lclangInstallAPI \
	-lclangInterpreter \
	-lclangFormat \
	-lclangHandleCXX \
	-lclangHandleLLVM \
	-lclangApplyReplacements \
	-lclangChangeNamespace \
	-lclangDoc \
	-lclangDocSupport \
	-lclangMove \
	-lclangQuery \
	-lclangReorderFields \
	-lclangIncludeCleaner \
	-lclangIncludeFixer \
	-lclangIncludeFixerPlugin \
	-lclangTransformer \
	-lclangUnifiedSymbolResolution \
	-lclangBasic
else
  # 系统安装 (Ubuntu LLVM 18)：全静态库，直接引用 .a 文件
  LLVM_LIBDIR := $(firstword $(shell $(LLVM_CONFIG) --ldflags | tr ' ' '\n' | grep '^-L' | head -1 | sed 's/^-L//'))
  LLVMLIBS := $(shell ls $(LLVM_LIBDIR)/libLLVM*.a 2>/dev/null | grep -viE 'polly|lto' | tr '\n' ' ')
  CLANGLIBS := $(shell ls $(LLVM_LIBDIR)/libclang*.a 2>/dev/null | grep -vi clangd | tr '\n' ' ') $(LLVM_LIBDIR)/libclangDriver.a
endif

PROJECTS = frontendAction IRMake
PROJECT_OBJECTS = frontendAction.o IRMake.o visitor.o report.o pipeline.o

default: all

all: $(PROJECTS)

%.o: $(SRC_DIR)/%.cpp
	@echo Compiling $*.cpp
	$(QUIET)$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $<

frontendAction: frontendAction.o visitor.o report.o pipeline.o
	@echo Linking $@
	$(QUIET)$(CXX) -o $@ $^ $(LDFLAGS) -Wl,--start-group $(CLANGLIBS) $(LLVMLIBS) $(SYSTEMLIBS) -Wl,--end-group

IRMake: IRMake.o
	@echo Linking $@
	$(QUIET)$(CXX) -o $@ $^ $(LDFLAGS) $(LLVMLIBS) $(SYSTEMLIBS)

test: frontendAction
	@echo "=== 测试 output/test.c ==="
	./frontendAction output/test.c
	@echo ""
	@echo "=== 测试 output/sum.c ==="
	./frontendAction output/sum.c

clean:
	$(QUIET)rm -f $(PROJECTS) $(PROJECT_OBJECTS)
