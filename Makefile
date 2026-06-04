LLVM_CONFIG = /home/li/llvm-project/build/bin/llvm-config

ifndef VERBOSE
QUIET:=@
endif

SRC_DIR?=$(PWD)

# 核心：LLVM 官方给出的所有编译参数
CXXFLAGS += $(shell $(LLVM_CONFIG) --cxxflags)
CXXFLAGS += $(shell $(LLVM_CONFIG) --cppflags)
CXXFLAGS += -fno-rtti

# 头文件路径
CPPFLAGS += -I/home/li/llvm-project/clang/include
CPPFLAGS += -I/home/li/llvm-project/build/tools/clang/include
CPPFLAGS += -I/home/li/llvm-project/build/include
CPPFLAGS += -I$(SRC_DIR)

# 链接选项
LDFLAGS += $(shell $(LLVM_CONFIG) --ldflags)

# 【关键】调整顺序：被依赖的库放在后面！
CLANGLIBS = \
	-lclangFrontend \
	-lclangFrontendTool \
	-lclangDriver \
	-lclangSerialization \
	-lclangCodeGen \
	-lclangParse \
	-lclangSema \
	-lclangAnalysis \
	-lclangAnalysisLifetimeSafety \
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
	-lclangScalableStaticAnalysisFrameworkFrontend \
	-lclangScalableStaticAnalysisFrameworkAnalyses \
	-lclangScalableStaticAnalysisFrameworkCore \
	-lclangScalableStaticAnalysisFrameworkTool \
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
	-lclangBasic  # 【关键】把 Basic 放在最后，因为它被所有库依赖！

# LLVM 库
LLVMLIBS   = $(shell $(LLVM_CONFIG) --libs all)
SYSTEMLIBS = $(shell $(LLVM_CONFIG) --system-libs)

PROJECTS = frontendAction IRMake
PROJECT_OBJECTS = frontendAction.o IRMake.o

default: all

all: $(PROJECTS)

%.o: $(SRC_DIR)/%.cpp
	@echo Compiling $*.cpp
	$(QUIET)$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $<

frontendAction: frontendAction.o
	@echo Linking $@
	$(QUIET)$(CXX) -o $@ $^ $(LDFLAGS) $(CLANGLIBS) $(LLVMLIBS) $(SYSTEMLIBS)

IRMake: IRMake.o
	@echo Linking $@
	$(QUIET)$(CXX) -o $@ $^ $(LDFLAGS) $(LLVMLIBS) $(SYSTEMLIBS)

test: frontendAction
	@echo "=== 测试 test.c ==="
	./frontendAction test.c
	@echo ""
	@echo "=== 测试 sum.c ==="
	./frontendAction sum.c

clean:
	$(QUIET)rm -f $(PROJECTS) $(PROJECT_OBJECTS)
