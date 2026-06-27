#ifndef VISITOR_H
#define VISITOR_H

#include "core_types.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"

// ====================== Clang 诊断收集器 ======================
class DiagCollector : public clang::DiagnosticConsumer {
    AnalysisResult &Result;
public:
    explicit DiagCollector(AnalysisResult &R);
    void HandleDiagnostic(clang::DiagnosticsEngine::Level DiagLevel,
                          const clang::Diagnostic &Info) override;
};

// ====================== 自定义 AST 访问者 ======================
class MyASTVisitor : public clang::RecursiveASTVisitor<MyASTVisitor> {
public:
    AnalysisStats &Stats;
    clang::SourceManager *SM = nullptr;
    clang::ASTContext *Ctx = nullptr;

    // 当前正在遍历的函数（用于逐函数统计）
    FunctionStats *CurrentFunc = nullptr;

    // 当前嵌套深度（if/for/while/do-while/switch）
    int currentDepth = 0;

    explicit MyASTVisitor(AnalysisStats &S);

    // Traverse 覆写：跟踪嵌套深度
    bool TraverseIfStmt(clang::IfStmt *IS);
    bool TraverseForStmt(clang::ForStmt *FS);
    bool TraverseWhileStmt(clang::WhileStmt *WS);
    bool TraverseDoStmt(clang::DoStmt *DS);
    bool TraverseSwitchStmt(clang::SwitchStmt *SS);
    bool TraverseFunctionDecl(clang::FunctionDecl *FD);

    // Visit 方法：计数 + 检查
    bool VisitFunctionDecl(clang::FunctionDecl *FD);
    bool VisitVarDecl(clang::VarDecl *VD);
    bool VisitIfStmt(clang::IfStmt *IS);
    bool VisitForStmt(clang::ForStmt *FS);
    bool VisitWhileStmt(clang::WhileStmt *WS);
    bool VisitDoStmt(clang::DoStmt *DS);
    bool VisitSwitchStmt(clang::SwitchStmt *SS);
    bool VisitSwitchCase(clang::SwitchCase *SC);
    bool VisitBreakStmt(clang::BreakStmt *BS);
    bool VisitContinueStmt(clang::ContinueStmt *CS);
    bool VisitReturnStmt(clang::ReturnStmt *RS);
    bool VisitGotoStmt(clang::GotoStmt *GS);
    bool VisitBinaryOperator(clang::BinaryOperator *BO);
    bool VisitConditionalOperator(clang::ConditionalOperator *CO);
    bool VisitCallExpr(clang::CallExpr *CE);

private:
    bool isFromMainFile(clang::Decl *D);
    bool isStmtFromMainFile(clang::Stmt *S);
    void checkEmptyBlock(clang::Stmt *Body, const std::string &Type);
};

// ====================== 自定义 AST Consumer ======================
class MyASTConsumer : public clang::ASTConsumer {
    AnalysisStats &Stats;
public:
    explicit MyASTConsumer(AnalysisStats &S);
    void HandleTranslationUnit(clang::ASTContext &Context) override;
};

#endif
