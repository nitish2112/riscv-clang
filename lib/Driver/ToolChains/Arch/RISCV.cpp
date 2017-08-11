//===--- RISCV.cpp - Tools Implementations ----------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "ToolChains/CommonArgs.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/Options.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Option/ArgList.h"

using namespace clang::driver;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

const char *riscv::getRISCVTargetCPU(const ArgList &Args,
                                     const llvm::Triple &Triple) {
  StringRef MArch;
  bool Is64Bit = Triple.getArch() == llvm::Triple::riscv64;

  if (Arg *A = Args.getLastArg(options::OPT_mriscv_EQ)) {
    // Otherwise, if we have -march= choose the base CPU for that arch.
    MArch = A->getValue();
  } else {
    // Otherwise, use the Arch from the triple.
    MArch = Triple.getArchName();
  }

  if (MArch.startswith("riscv32ema"))
    return "rv32ema";

  if (MArch.startswith("riscv32imac"))
    return "rv32imac";

  if (MArch.startswith("riscv64imac"))
    return "rv64imac";

  if (Is64Bit)
    return "generic-rv64";

  return "generic-rv32";
}
