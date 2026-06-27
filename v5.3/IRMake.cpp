#include <llvm/ADT/SmallVector.h>//这是为了引入SmallVector<>模板，这个数据结构帮助我们构建高效的向量，当元素数量不大的时候。查看 http://llvm.org/docs/ProgrammersManual.html 关于LLVM数据结构的介绍。
#include <llvm/IR/Verifier.h>//验证Pass是一个重要的分析，检查你的LLVM模块是否恰当地被构建，遵从IR规则。
#include <llvm/IR/BasicBlock.h>//这个头文件声明BasicBlock类，这是我们已经介绍过的重要的IR实体。
#include <llvm/IR/CallingConv.h>//这个头文件定义函数调用用到的一套ABI规则，例如在何处存储函数参数。
#include <llvm/IR/Function.h>//这个头文件声明Function类，一种IR实体。
#include <llvm/IR/Instructions.h>//这个头文件声明Instruction类的所有子类，一种基本的IR数据结构。
#include <llvm/IR/LLVMContext.h>//这个头文件存储LLVM程序库的全局域数据，每个线程使用不同的context，让多线程实现正确工作。
#include <llvm/IR/Module.h>//这个头文件声明Module类，IR层级结构的顶层实体。
#include <llvm/Bitcode/BitcodeWriter.h>//这个头文件为我们提供了读写LLVM bitcode文件的代码。
#include <llvm/Support/ToolOutputFile.h>//这个头文件声明了一个辅助类，用以写输出文件。
#include <llvm/Support/FileSystem.h>//这个头文件提供了与文件系统交互的函数和类，例如创建、删除、检查文件等。

using namespace llvm;

// int main() {
//     LLVMContext Context;//创建一个LLVMContext对象，所有LLVM IR的构建都需要这个上下文。
//     Module *M = new Module("my_module", Context);//创建一个Module对象，表示我们要构建的IR模块。

//     // 创建一个函数：int add(int a, int b)
//     FunctionType *FuncType = FunctionType::get(Type::getInt32Ty(Context), 
//                                                {Type::getInt32Ty(Context), Type::getInt32Ty(Context)}, 
//                                                false);
//     Function *AddFunction = Function::Create(FuncType, Function::ExternalLinkage, "add", M);

//     // 创建一个基本块并将其添加到函数中
//     BasicBlock *BB = BasicBlock::Create(Context, "entry", AddFunction);
    
//     // 获取函数参数
//     auto Args = AddFunction->args();
//     auto ArgIter = Args.begin();
//     Value *A = &*ArgIter; // 第一个参数
//     Value *B = &*(++ArgIter); // 第二个参数

//     // 创建一个加法指令：result = a + b
//     Value *Result = BinaryOperator::CreateAdd(A, B, "result", BB);

//     // 创建一个返回指令：return result
//     ReturnInst::Create(Context, Result, BB);

//     // 验证生成的模块是否正确
//     if (verifyModule(*M, &errs())) {
//         errs() << "Error constructing module!\n";
//         return 1;
//     }

//     // 输出生成的IR到标准输出
//     M->print(outs(), nullptr);

//     delete M; // 清理内存
//     return 0;
// }
LLVMContext Context;

Module *makeLLVMModules(){

    Module *mod = new Module("sum.ll", Context);

    mod->setDataLayout("e-m:e-i64:64-f80:128-n8:16:32:64-S128");
    mod->setTargetTriple(Triple("x86_64-unknown-linux-gnu"));
    SmallVector<Type *,2 >FuncTy_1_args;
    FuncTy_1_args.push_back(IntegerType::get(mod->getContext(),32));
    FuncTy_1_args.push_back(IntegerType::get(mod->getContext(),32));
    FunctionType *FuncTy_1=FunctionType::get(IntegerType::get(mod->getContext(),32),FuncTy_1_args,false);
    Function *Func_1=Function::Create(FuncTy_1,GlobalValue::ExternalLinkage,"add",mod);
    Func_1->setCallingConv(CallingConv::C);
    Function::arg_iterator Func_1_arg_iter=Func_1->arg_begin();
    Value *int32_a=&*Func_1_arg_iter;
    int32_a->setName("a");
    Value *int32_b=&*(++Func_1_arg_iter);
    int32_b->setName("b");
    BasicBlock *BB_1=BasicBlock::Create(mod->getContext(),"entry",Func_1);
    AllocaInst *ptrA=new AllocaInst(IntegerType::get(Context,32),0, nullptr,  Align(4),"a.addr",BB_1);

    AllocaInst *ptrB=new AllocaInst(IntegerType::get(Context,32),0, nullptr,  Align(4),"b.addr",BB_1);

    StoreInst *storeA=new StoreInst(int32_a,ptrA,false,Align(4),BB_1);

    StoreInst *storeB=new StoreInst(int32_b,ptrB,false,Align(4),BB_1);

    LoadInst *loadA=new LoadInst(ptrA->getAllocatedType(),ptrA,"", false,Align(4),BB_1);

    LoadInst *loadB=new LoadInst(ptrB->getAllocatedType(),ptrB,"", false,Align(4),BB_1);

    BinaryOperator *result = BinaryOperator::CreateAdd(loadA, loadB, "result", BB_1);

    ReturnInst *ret=ReturnInst::Create(mod->getContext(),result,BB_1);

    return mod;
}

int main(){
    Module *mod=makeLLVMModules();
    if (verifyModule(*mod, &errs())) {
        errs() << "模块错误!\n";
        return 1;
    }
    std::error_code EC;
    // sys::fs::OpenFlags Flags = sys::fs::CD_CreateAlways;
    llvm::ToolOutputFile out("sum.bc", EC, sys::fs::OF_None);
    if(EC){
        errs()<<"Could not open file: "<<EC.message();
        return 1;
    }
    WriteBitcodeToFile(*mod,out.os());
    out.keep();
    delete mod;
    return 0;
}