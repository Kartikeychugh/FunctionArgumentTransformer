#define DEBUG_TYPE "FunctionArgumentTransformer"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/ADT/gitArrayRef.h"
#include "llvm/IR/Constants.h"
#include <map>
#include <set>
#include <string>
#include <sstream>

using namespace llvm;
namespace
{
    struct FunctionArgumentTransformer : public ModulePass
    {
        static char ID;
        std::set<Function *> clonedFunctions;
        std::map<std::string, int> functionMappings;
        FunctionArgumentTransformer() : ModulePass(ID) {}

    private:
        void PrintConstantInt(ConstantInt *CI)
        {
            errs() << "ConstantInt: " << CI->getSExtValue() << "\n";
        }

        bool IsConstantArgument(Value *arg_value)
        {
            if (isa<Constant>(arg_value) && dyn_cast<ConstantInt>(arg_value))
                return true;
            return false;
        }

        bool NeedsToBeCloned(CallInst *call_inst, Function *calledFunction)
        {
            // Step 2
            for (unsigned i = 0; i < call_inst->getNumArgOperands(); i++)
            {
                Value *arg_value = call_inst->getArgOperand(i);
                // Step 3
                if (IsConstantArgument(arg_value))
                {
                    return clonedFunctions.find(calledFunction) == clonedFunctions.end();
                }
            }

            return false;
        }

        std::string GenerateClonedFunctionName(Function *calledFunction)
        {
            std::string calledFunctionName = calledFunction->getName().str();
            if (functionMappings.find(calledFunctionName) == functionMappings.end())
            {
                functionMappings[calledFunctionName] = 1;
            }
            else
            {
                functionMappings[calledFunctionName]++;
            }
            std::ostringstream ss;
            ss << functionMappings[calledFunctionName];
            return calledFunctionName + ss.str();
        }

        Function *SetupClone(Function *calledFunction)
        {
            ValueToValueMapTy VMap;
            Function *clonedFunction = CloneFunction(calledFunction, VMap, false);
            std::string newName = GenerateClonedFunctionName(calledFunction);
            clonedFunction->setName(newName);
            // clonedFunction->setLinkage(GlobalValue::InternalLinkage);
            calledFunction->getParent()->getFunctionList().push_back(clonedFunction);
            return clonedFunction;
        }

        void ReplaceFunctionCall(CallInst *call_inst, Function *clonedFunction)
        {
            call_inst->setCalledFunction(clonedFunction);
        }

        void ReplaceArgumentsWithConstants(CallInst *call_inst, Function *clonedFunction)
        {
            Function::arg_iterator itr = clonedFunction->arg_begin();
            for (unsigned i = 0; i < call_inst->getNumArgOperands(); i++, itr++)
            {
                Value *arg_value = call_inst->getArgOperand(i);
                // Step 6
                if (ConstantInt *CI = dyn_cast<ConstantInt>(arg_value))
                {
                    // Step 7
                    itr->replaceAllUsesWith(CI);
                }
            }
        }

        virtual bool runOnModule(Module &M) override
        {
            for (Module::iterator FuncIt = M.getFunctionList().begin(); FuncIt != M.getFunctionList().end(); FuncIt++)
            {
                Function *F = FuncIt;
                for (Function::iterator BBIT = F->begin(); BBIT != F->end(); BBIT++)
                {

                    BasicBlock *BB = BBIT;
                    for (BasicBlock::iterator IIT = BB->begin(); IIT != BB->end(); IIT++)
                    {
                        // Step 1
                        Instruction *I = IIT;
                        if (isa<CallInst>(I))
                        {
                            CallInst *call_inst = cast<CallInst>(I);
                            Function *calledFunction = call_inst->getCalledFunction();
                            // Step 2 and 3
                            if (NeedsToBeCloned(call_inst, calledFunction))
                            {
                                errs() << "Need to be cloned: " << calledFunction->getName() << "\n";
                                // Step 4
                                Function *clonedFunction = SetupClone(calledFunction);
                                // Step 5
                                ReplaceFunctionCall(call_inst, clonedFunction);
                                // Step 6 and 7
                                ReplaceArgumentsWithConstants(call_inst, clonedFunction);
                                clonedFunctions.insert(clonedFunction);
                            }
                            else
                            {
                                errs() << "Doesn't need to be cloned: " << calledFunction->getName() << "\n";
                            }
                        }
                    }
                }
            }

            return false;
        }
    };
}

char FunctionArgumentTransformer::ID = 0;
static RegisterPass<FunctionArgumentTransformer> X("argumentTransform", "function argument instantiation pass");

// ruitang@umd.edu