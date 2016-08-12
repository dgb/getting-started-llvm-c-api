/**
 * LLVM equivalent of:
 *
 * int sum(int a, int b) {
 *     return a + b;
 * }
 */

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char const *argv[]) {
    LLVMModuleRef mod = LLVMModuleCreateWithName("my_module");

    char *error = NULL;
    LLVMVerifyModule(mod, LLVMAbortProcessAction, &error);
    LLVMDisposeMessage(error);

    LLVMExecutionEngineRef engine;
    error = NULL;
    LLVMLinkInMCJIT();
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    if (LLVMCreateExecutionEngineForModule(&engine, mod, &error) != 0) {
        fprintf(stderr, "failed to create execution engine\n");
        abort();
    }
    if (error) {
        fprintf(stderr, "error: %s\n", error);
        LLVMDisposeMessage(error);
        exit(EXIT_FAILURE);
    }

    if (argc < 3) {
        fprintf(stderr, "usage: %s x y\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    long long x = strtoll(argv[1], NULL, 10);
    long long y = strtoll(argv[2], NULL, 10);

    // Builder
    LLVMBuilderRef builder = LLVMCreateBuilder();

    // Sum function int32, int32 -> int32
    LLVMTypeRef param_types[] = { LLVMInt32Type(), LLVMInt32Type() };
    LLVMTypeRef ret_type = LLVMFunctionType(LLVMInt32Type(), param_types, 2, 0);
    LLVMValueRef sum = LLVMAddFunction(mod, "sum", ret_type);

    LLVMBasicBlockRef sum_entry = LLVMAppendBasicBlock(sum, "entry");

    LLVMPositionBuilderAtEnd(builder, sum_entry);

    LLVMValueRef tmp = LLVMBuildAdd(builder, LLVMGetParam(sum, 0), LLVMGetParam(sum, 1), "tmp");
    LLVMBuildRet(builder, tmp);

    // Wrapper function void -> int32
    LLVMTypeRef wrap_type = LLVMFunctionType(LLVMInt32Type(), NULL, 0, 0);
    LLVMValueRef wrap = LLVMAddFunction(mod, "wrap", wrap_type);

    LLVMBasicBlockRef wrap_entry = LLVMAppendBasicBlock(wrap, "entry");
    LLVMPositionBuilderAtEnd(builder, wrap_entry);

    LLVMValueRef args[] = {
        LLVMConstInt(LLVMInt32Type(), x, 0),
        LLVMConstInt(LLVMInt32Type(), y, 0)
    };

    LLVMValueRef wrap_tmp = LLVMBuildCall(builder, sum, args, 2, "wrap_tmp");

    LLVMBuildRet(builder, wrap_tmp);

    LLVMGenericValueRef res = LLVMRunFunction(engine, wrap, 0, NULL);
    printf("%d\n", (int)LLVMGenericValueToInt(res, 0));

    // Write out bitcode to file
    if (LLVMWriteBitcodeToFile(mod, "sum.bc") != 0) {
        fprintf(stderr, "error writing bitcode to file, skipping\n");
    }

    LLVMDisposeBuilder(builder);
    LLVMDisposeExecutionEngine(engine);
}
