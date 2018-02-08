//===---- CGLoopInfo.cpp - LLVM CodeGen for loop metadata -*- C++ -*-------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "CGLoopInfo.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/Sema/LoopHint.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
using namespace clang::CodeGen;
using namespace llvm;

static MDNode *createMetadata(LLVMContext &Ctx, const LoopAttributes &Attrs,
                              const llvm::DebugLoc &StartLoc,
                              const llvm::DebugLoc &EndLoc) {

  if (!Attrs.IsParallel && Attrs.VectorizeWidth == 0 &&
      Attrs.InterleaveCount == 0 && Attrs.UnrollCount == 0 &&
      Attrs.UnorderedForCount == 0 && // nitish: unordered_for
      Attrs.FeederCount == 0 && // nitish: feeder
      Attrs.VectorizeEnable == LoopAttributes::Unspecified &&
      // nitish: add support for unordered_for and feeder
      Attrs.UnorderedForEnable == LoopAttributes::Unspecified &&
      Attrs.FeederEnable == LoopAttributes::Unspecified &&

      Attrs.UnrollEnable == LoopAttributes::Unspecified &&
      Attrs.DistributeEnable == LoopAttributes::Unspecified &&
      !StartLoc && !EndLoc)
    return nullptr;

  SmallVector<Metadata *, 4> Args;
  // Reserve operand 0 for loop id self reference.
  auto TempNode = MDNode::getTemporary(Ctx, None);
  Args.push_back(TempNode.get());

  // If we have a valid start debug location for the loop, add it.
  if (StartLoc) {
    Args.push_back(StartLoc.getAsMDNode());

    // If we also have a valid end debug location for the loop, add it.
    if (EndLoc)
      Args.push_back(EndLoc.getAsMDNode());
  }

  // Setting vectorize.width
  if (Attrs.VectorizeWidth > 0) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.vectorize.width"),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt32Ty(Ctx), Attrs.VectorizeWidth))};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  // Setting interleave.count
  if (Attrs.InterleaveCount > 0) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.interleave.count"),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt32Ty(Ctx), Attrs.InterleaveCount))};
    Args.push_back(MDNode::get(Ctx, Vals));
  }


  // nitish: unordered_for
  if (Attrs.UnorderedForCount > 0) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.unordered_for.count"),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt32Ty(Ctx), Attrs.UnorderedForCount))};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  // nitish: feeder
  if (Attrs.FeederCount > 0) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.feeder.count"),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt32Ty(Ctx), Attrs.FeederCount))};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  // Setting interleave.count
  if (Attrs.UnrollCount > 0) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.unroll.count"),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt32Ty(Ctx), Attrs.UnrollCount))};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  // Setting vectorize.enable
  if (Attrs.VectorizeEnable != LoopAttributes::Unspecified) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.vectorize.enable"),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt1Ty(Ctx), (Attrs.VectorizeEnable ==
                                                   LoopAttributes::Enable)))};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  // nitish: adding support for pragma "unordered_for"
  if (Attrs.UnorderedForEnable != LoopAttributes::Unspecified) {                             
    std::string Name;
    if (Attrs.UnorderedForEnable == LoopAttributes::Enable)
      Name = "llvm.loop.unordered_for.enable";                         
    else if (Attrs.UnorderedForEnable == LoopAttributes::Full)
      Name = "llvm.loop.unordered_for.full";   
    else
      Name = "llvm.loop.unordered_for.disable";   
    Metadata *Vals[] = {MDString::get(Ctx, Name)};                            
    Args.push_back(MDNode::get(Ctx, Vals));                                   
  }

  // nitish: adding support for pragma "feeder"
  if (Attrs.FeederEnable != LoopAttributes::Unspecified) {                             
    std::string Name;
    if (Attrs.FeederEnable == LoopAttributes::Enable)
      Name = "llvm.loop.feeder.enable";                         
    else if (Attrs.FeederEnable == LoopAttributes::Full)
      Name = "llvm.loop.feeder.full";   
    else
      Name = "llvm.loop.feeder.disable";   
    Metadata *Vals[] = {MDString::get(Ctx, Name)};                            
    Args.push_back(MDNode::get(Ctx, Vals));                                   
  }

  // Setting unroll.full or unroll.disable
  if (Attrs.UnrollEnable != LoopAttributes::Unspecified) {
    std::string Name;
    if (Attrs.UnrollEnable == LoopAttributes::Enable)
      Name = "llvm.loop.unroll.enable";
    else if (Attrs.UnrollEnable == LoopAttributes::Full)
      Name = "llvm.loop.unroll.full";
    else
      Name = "llvm.loop.unroll.disable";
    Metadata *Vals[] = {MDString::get(Ctx, Name)};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  if (Attrs.DistributeEnable != LoopAttributes::Unspecified) {
    Metadata *Vals[] = {MDString::get(Ctx, "llvm.loop.distribute.enable"),
                        ConstantAsMetadata::get(ConstantInt::get(
                            Type::getInt1Ty(Ctx), (Attrs.DistributeEnable ==
                                                   LoopAttributes::Enable)))};
    Args.push_back(MDNode::get(Ctx, Vals));
  }

  // Set the first operand to itself.
  MDNode *LoopID = MDNode::get(Ctx, Args);
  LoopID->replaceOperandWith(0, LoopID);
  return LoopID;
}

LoopAttributes::LoopAttributes(bool IsParallel)
    : IsParallel(IsParallel), VectorizeEnable(LoopAttributes::Unspecified),
      UnorderedForEnable(LoopAttributes::Unspecified), // nitish: unordered_for
      FeederEnable(LoopAttributes::Unspecified), // nitish: feeder
      UnrollEnable(LoopAttributes::Unspecified), VectorizeWidth(0),
      InterleaveCount(0), UnrollCount(0), UnorderedForCount(0), FeederCount(0), // nitish: unordered_for and feeder
      DistributeEnable(LoopAttributes::Unspecified) {}

void LoopAttributes::clear() {
  IsParallel = false;
  VectorizeWidth = 0;
  InterleaveCount = 0;
  // nitish: unordered_for
  UnorderedForCount = 0;
  // nitish: feeder
  FeederCount = 0;
  UnrollCount = 0;
  VectorizeEnable = LoopAttributes::Unspecified;
  // nitish: unordered_for
  UnorderedForEnable = LoopAttributes::Unspecified;
  // nitish: feeder
  FeederEnable = LoopAttributes::Unspecified;
  UnrollEnable = LoopAttributes::Unspecified;
  DistributeEnable = LoopAttributes::Unspecified;
}

LoopInfo::LoopInfo(BasicBlock *Header, const LoopAttributes &Attrs,
                   const llvm::DebugLoc &StartLoc, const llvm::DebugLoc &EndLoc)
    : LoopID(nullptr), Header(Header), Attrs(Attrs) {
  LoopID = createMetadata(Header->getContext(), Attrs, StartLoc, EndLoc);
}

void LoopInfoStack::push(BasicBlock *Header, const llvm::DebugLoc &StartLoc,
                         const llvm::DebugLoc &EndLoc) {
  Active.push_back(LoopInfo(Header, StagedAttrs, StartLoc, EndLoc));
  // Clear the attributes so nested loops do not inherit them.
  StagedAttrs.clear();
}

void LoopInfoStack::push(BasicBlock *Header, clang::ASTContext &Ctx,
                         ArrayRef<const clang::Attr *> Attrs,
                         const llvm::DebugLoc &StartLoc,
                         const llvm::DebugLoc &EndLoc) {

  // Identify loop hint attributes from Attrs.
  for (const auto *Attr : Attrs) {
    const LoopHintAttr *LH = dyn_cast<LoopHintAttr>(Attr);
    const OpenCLUnrollHintAttr *OpenCLHint =
        dyn_cast<OpenCLUnrollHintAttr>(Attr);

    // Skip non loop hint attributes
    if (!LH && !OpenCLHint) {
      continue;
    }

    LoopHintAttr::OptionType Option = LoopHintAttr::Unroll;
    LoopHintAttr::LoopHintState State = LoopHintAttr::Disable;
    unsigned ValueInt = 1;
    // Translate opencl_unroll_hint attribute argument to
    // equivalent LoopHintAttr enums.
    // OpenCL v2.0 s6.11.5:  
    // 0 - full unroll (no argument).
    // 1 - disable unroll.
    // other positive integer n - unroll by n.
    if (OpenCLHint) {
      ValueInt = OpenCLHint->getUnrollHint();
      if (ValueInt == 0) {
        State = LoopHintAttr::Full;
      } else if (ValueInt != 1) {
        Option = LoopHintAttr::UnrollCount;
        State = LoopHintAttr::Numeric;
      }
    } else if (LH) {
      auto *ValueExpr = LH->getValue();
      if (ValueExpr) {
        llvm::APSInt ValueAPS = ValueExpr->EvaluateKnownConstInt(Ctx);
        ValueInt = ValueAPS.getSExtValue();
      }

      Option = LH->getOption();
      State = LH->getState();
    }
    switch (State) {
    case LoopHintAttr::Disable:
      switch (Option) {
      case LoopHintAttr::Vectorize:
        // Disable vectorization by specifying a width of 1.
        setVectorizeWidth(1);
        break;
      case LoopHintAttr::Interleave:
        // Disable interleaving by speciyfing a count of 1.
        setInterleaveCount(1);
        break;
       // nitish: Added support for unordered_for
      case LoopHintAttr::UnorderedFor:
        setUnorderedForState(LoopAttributes::Disable);
        break;
       // nitish: Added support for feeder
      case LoopHintAttr::Feeder:
        setFeederState(LoopAttributes::Disable);
        break;
      case LoopHintAttr::Unroll:
        setUnrollState(LoopAttributes::Disable);
        break;
      case LoopHintAttr::Distribute:
        setDistributeState(false);
        break;
      case LoopHintAttr::UnrollCount:
      case LoopHintAttr::UnorderedForCount: // nitish: unordered_for
      case LoopHintAttr::FeederCount: // nitish: feeder
      case LoopHintAttr::VectorizeWidth:
      case LoopHintAttr::InterleaveCount:
        llvm_unreachable("Options cannot be disabled.");
        break;
      }
      break;
    case LoopHintAttr::Enable:
      switch (Option) {
      case LoopHintAttr::Vectorize:
      case LoopHintAttr::Interleave:
        setVectorizeEnable(true);
        break;
      // nitish: Added support for unordered_for
      case LoopHintAttr::UnorderedFor:
        setUnorderedForState(LoopAttributes::Enable);
        break;
      // nitish: Added support for feeder
      case LoopHintAttr::Feeder:
        setFeederState(LoopAttributes::Enable);
        break;
      case LoopHintAttr::Unroll:
        setUnrollState(LoopAttributes::Enable);
        break;
      case LoopHintAttr::Distribute:
        setDistributeState(true);
        break;
      case LoopHintAttr::UnrollCount:
      case LoopHintAttr::UnorderedForCount: // nitish: unordered_for
      case LoopHintAttr::FeederCount: // nitish: feeder
      case LoopHintAttr::VectorizeWidth:
      case LoopHintAttr::InterleaveCount:
        llvm_unreachable("Options cannot enabled.");
        break;
      }
      break;
    case LoopHintAttr::AssumeSafety:
      switch (Option) {
      case LoopHintAttr::Vectorize:
      case LoopHintAttr::Interleave:
        // Apply "llvm.mem.parallel_loop_access" metadata to load/stores.
        setParallel(true);
        setVectorizeEnable(true);
        break;
      // nitish: added support for unordered_for
      case LoopHintAttr::UnorderedFor:
      case LoopHintAttr::UnorderedForCount:
      // nitish: added support for feeder
      case LoopHintAttr::Feeder:
      case LoopHintAttr::FeederCount:

      case LoopHintAttr::Unroll:
      case LoopHintAttr::UnrollCount:
      case LoopHintAttr::VectorizeWidth:
      case LoopHintAttr::InterleaveCount:
      case LoopHintAttr::Distribute:
        llvm_unreachable("Options cannot be used to assume mem safety.");
        break;
      }
      break;
    case LoopHintAttr::Full:
      switch (Option) {
      // nitish: add support for unordered_for
      case LoopHintAttr::UnorderedFor:
        setUnorderedForState(LoopAttributes::Full);
        break;
      // nitish: add support for feeder
      case LoopHintAttr::Feeder:
        setFeederState(LoopAttributes::Full);
        break;
      case LoopHintAttr::Unroll:
        setUnrollState(LoopAttributes::Full);
        break;
      case LoopHintAttr::Vectorize:
      case LoopHintAttr::Interleave:
      case LoopHintAttr::UnorderedForCount: // nitish: unordered_for
      case LoopHintAttr::FeederCount: // nitish: feeder
      case LoopHintAttr::UnrollCount:
      case LoopHintAttr::VectorizeWidth:
      case LoopHintAttr::InterleaveCount:
      case LoopHintAttr::Distribute:
        llvm_unreachable("Options cannot be used with 'full' hint.");
        break;
      }
      break;
    case LoopHintAttr::Numeric:
      switch (Option) {
      case LoopHintAttr::VectorizeWidth:
        setVectorizeWidth(ValueInt);
        break;
      case LoopHintAttr::InterleaveCount:
        setInterleaveCount(ValueInt);
        break;
      // nitish: unordered_for
      case LoopHintAttr::UnorderedForCount:
        setUnorderedForCount(ValueInt);
        break;
      // nitish: feeder
      case LoopHintAttr::FeederCount:
        setFeederCount(ValueInt);
        break;
      case LoopHintAttr::UnrollCount:
        setUnrollCount(ValueInt);
        break;
      case LoopHintAttr::UnorderedFor: // nitish: unordered_for
      case LoopHintAttr::Feeder: // nitish: feeder
      case LoopHintAttr::Unroll:
      case LoopHintAttr::Vectorize:
      case LoopHintAttr::Interleave:
      case LoopHintAttr::Distribute:
        llvm_unreachable("Options cannot be assigned a value.");
        break;
      }
      break;
    }
  }

  /// Stage the attributes.
  push(Header, StartLoc, EndLoc);
}

void LoopInfoStack::pop() {
  assert(!Active.empty() && "No active loops to pop");
  Active.pop_back();
}

void LoopInfoStack::InsertHelper(Instruction *I) const {
  if (!hasInfo())
    return;

  const LoopInfo &L = getInfo();
  if (!L.getLoopID())
    return;

  if (TerminatorInst *TI = dyn_cast<TerminatorInst>(I)) {
    for (unsigned i = 0, ie = TI->getNumSuccessors(); i < ie; ++i)
      if (TI->getSuccessor(i) == L.getHeader()) {
        TI->setMetadata(llvm::LLVMContext::MD_loop, L.getLoopID());
        break;
      }
    return;
  }

  if (L.getAttributes().IsParallel && I->mayReadOrWriteMemory())
    I->setMetadata("llvm.mem.parallel_loop_access", L.getLoopID());
}
