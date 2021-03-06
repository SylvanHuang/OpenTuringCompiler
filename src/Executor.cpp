//
//  Executor.cpp
//  Turing Compiler
//
//  Created by Tristan Hume on 12-02-05.
//  Copyright 2012 15 Norwich Way. All rights reserved.
//

#include "Executor.h"

#include <iostream>
#include <sstream>

#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/ExecutionEngine/JITMemoryManager.h>
#include <llvm/Target/TargetOptions.h>

#include "TuringCommon/LibDefs.h"
#include "Message.h"

using namespace llvm;

static std::ostringstream errorBuffer;
static void MyFlushErrorBuffer() {
    Message::error(errorBuffer.str());
    errorBuffer.str("");
}
//! this gets set as the standard error stream
//! if it is passed a string without a newline it is added to the buffer
//! when it sees a newline it takes the buffer and writes it to a Message::error
void Executor_RuntimeRuntimeErrorStream(TInt streamNum, const char *error) {
    std::string errMsg(error);
    
    // consume newline-separated messages and write them as Message::error
    size_t lastFound = 0;
    size_t found = errMsg.find_first_of("\n");
    while (found!=std::string::npos)
    {
        errorBuffer << errMsg.substr(lastFound,found);
        MyFlushErrorBuffer();
        lastFound = found + 1; // +1 to skip over \n
        found = errMsg.find_first_of("\n",found+1);
    }
    errorBuffer << errMsg.substr(lastFound); // add the rest of the string to the buffer
}

Executor::Executor(Module *mod, TuringCommon::StreamManager *streamManager,
                   LibManager *libManager, const std::string &executionDir) : StallOnEnd(false), TheModule(mod), TheStreamManager(streamManager), TheLibManager(libManager), ExecutionDir(executionDir) {
    // add the error stream
    TInt errStream = TheStreamManager->registerStream(&Executor_RuntimeRuntimeErrorStream, NULL);
    TheStreamManager->setSpecialStream(-3, errStream);
    // do a crapton of JIT initialization
    InitializeNativeTarget();
    //llvm::JITExceptionHandling = true;
    std::string errStr;    
    llvm::EngineBuilder factory(TheModule);
    factory.setEngineKind(llvm::EngineKind::JIT);
    factory.setAllocateGVsWithCode(false);
    factory.setOptLevel(CodeGenOpt::Aggressive);
    factory.setErrorStr(&errStr);
    // Setup exception handling
    llvm::TargetOptions opts;
    opts.JITExceptionHandling = true;
    factory.setTargetOptions(opts);
    
    TheExecutionEngine = factory.create();
    if (!TheExecutionEngine) {
        fprintf(stderr, "Could not create ExecutionEngine: %s\n", errStr.c_str());
        exit(1);
    }
    
    
}

void Executor::optimize() {
    PassManager p;
    
    
    p.add(new TargetData(TheModule));
    p.add(createVerifierPass());
    
    // constant folding
    p.add(createConstantMergePass());
    // Provide basic AliasAnalysis support for GVN.
    p.add(createBasicAliasAnalysisPass());
    // inline functions
    p.add(createFunctionInliningPass());
    // Promote allocas to registers.
    p.add(createPromoteMemoryToRegisterPass());
    // Do simple "peephole" optimizations and bit-twiddling optzns.
    p.add(createInstructionCombiningPass());
    // Reassociate expressions.
    p.add(createReassociatePass());
    // Eliminate Common SubExpressions.
    p.add(createGVNPass());
    // get rid of extra copying
    p.add(createMemCpyOptPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc).
    p.add(createCFGSimplificationPass());
    
    // all the important stuff
    //StandardPass::AddPassesFromSet(&p,StandardPass::Module);
    
    // Run these optimizations on our Module
    bool changed = p.run(*TheModule);
#ifdef DEBUG_PRINT_BYTECODE
    if (changed) {
        Message::log("Optimized code:");
        TheModule->dump();
    }
#endif
    
}

bool Executor::run() {
    Function *mainFunc = TheModule->getFunction("main");
    
    if (!mainFunc) {
        Message::error("No main function found for execution");
        return false;
    }
    
    // open windows, pass stream managers, etc...
    for (unsigned int i = 0; i < TheLibManager->InitRunFunctions.size(); ++i) {
        LibManager::InitRunFunction initFunc = TheLibManager->InitRunFunctions[i];
        (*initFunc)(ExecutionDir.c_str(),TheStreamManager); // call function pointer
    }
    void *funcPtr = TheExecutionEngine->getPointerToFunction(mainFunc);
    void (*programMain)(TuringCommon::StreamManager*) = 
        (void (*)(TuringCommon::StreamManager*))(intptr_t)funcPtr; // cast it into a function
    
    try {
        programMain(TheStreamManager);
    } catch (TInt errCode) {
        Message::error(Twine("Execution failed with error code ") + Twine(errCode));
    }
    
    // maybe stall
    if (StallOnEnd) {
        stall();
    }
    
    // close windows and stuff
    for (unsigned int i = 0; i < TheLibManager->FinalizeRunFunctions.size(); ++i) {
        LibManager::FinalizeRunFunction finalizeFunc = TheLibManager->FinalizeRunFunctions[i];
        (*finalizeFunc)(); // call function pointer
    }
    
    return true;
}

void Executor::stall() {
    while (true) {
        // call periodic callbacks like UI active
        for (unsigned int i = 0; i < TheLibManager->PeriodicCallbackFunctions.size(); ++i) {
            LibManager::PeriodicCallbackFunction periodicFunc = TheLibManager->PeriodicCallbackFunctions[i];
            (*periodicFunc)(); // call function pointer
        }
        // sleep for a bit
        usleep(1000);
    }
}