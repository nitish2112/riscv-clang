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

  if (Arg *A = Args.getLastArg(options::OPT_mriscv_EQ)) {
    // Otherwise, if we have -march= choose the base CPU for that arch.
    MArch = A->getValue();
  } else {
    // Otherwise, use the Arch from the triple.
    MArch = Triple.getArchName();
  }

  if (MArch.startswith("riscv32ema"))
    return "rv32ema";

  if (MArch.startswith("riscv32emac"))
    return "rv32emac";

  if (MArch.startswith("riscv32imac"))
    return "rv32imac";

  if (MArch.startswith("riscv64imac"))
    return "rv64imac";

  bool Is64Bit = Triple.getArch() == llvm::Triple::riscv64;

  if (Arg *A = Args.getLastArg(options::OPT_march_EQ)) {
    StringRef MArch = A->getValue();
    Is64Bit = MArch.startswith("rv64");
  }

  if (Is64Bit)
    return "generic-rv64";

  return "generic-rv32";
}

void riscv::getRISCVTargetFeatures(const Driver &D, const ArgList &Args,
                                   const llvm::Triple &Triple,
                                   std::vector<StringRef> &Features) {
  StringRef MArch;

  if (Arg *A = Args.getLastArg(options::OPT_march_EQ)) {
    MArch = A->getValue();
  } else {
    // Otherwise, use the Arch from the triple.
    MArch = Triple.getArchName();
    if (MArch.startswith("riscv32e")) {
      Features.push_back("+e");
      Features.push_back("+rv32");
    } else if (MArch.startswith("riscv32")) {
      Features.push_back("+rv32");
      Features.push_back("+m");
      Features.push_back("+a");
    } else if (MArch.startswith("riscv64")) {
      Features.push_back("+rv64");
      Features.push_back("+m");
      Features.push_back("+a");
    } else {
      // ????
      D.Diag(diag::err_drv_invalid_arch_name) << MArch;
    }
    return;
  }

  bool hasF = false, hasD = false, hasE = false;

  if (MArch.startswith("rv32e")) {
    Features.push_back("+e");
    Features.push_back("+rv32");
    Features.push_back("-rv64");
    hasE = true;
  } else if (MArch.startswith("rv32i")) {
    Features.push_back("+rv32");
    Features.push_back("-rv64");
  } else if (MArch.startswith("rv64i")) {
    Features.push_back("+rv64");
    Features.push_back("-rv32");
  } else {
    D.Diag(diag::err_drv_invalid_arch_name) << MArch;
  }

  for (auto ext : MArch.substr(5)) {
    switch (ext) {
    case 'm':
      Features.push_back("+m");
      break;
    case 'a':
      Features.push_back("+a");
      break;
    case 'f':
      Features.push_back("+f");
      hasF = true;
      break;
    case 'd':
      Features.push_back("+d");
      hasD = true;
      break;
    case 'q':
      D.Diag(diag::err_drv_invalid_arch_name) << MArch;
      break;
    case 'c':
      Features.push_back("+c");
      break;
    case 'g':
      Features.push_back("+m");
      Features.push_back("+a");
      Features.push_back("+f");
      Features.push_back("+d");
      hasF = true;
      hasD = true;
      break;
    default:
      D.Diag(diag::err_drv_invalid_arch_name) << MArch;
    }
  }

  // Dependency check
  if (hasD && !hasF)
    D.Diag(diag::err_drv_invalid_arch_name) << MArch;

  if (hasE && (hasF || hasD))
    D.Diag(diag::err_drv_invalid_arch_name) << MArch;
}
