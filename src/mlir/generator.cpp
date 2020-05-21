// Copyright (c) Microsoft Corporation. All rights reserved.
// This file is licensed under the MIT license.

#include "generator.h"

namespace mlir::verona
{
  void Generator::readAST(::ast::Ast& ast)
  {
    // TODO: Convert AST into MLIR module
    throw std::runtime_error("Parsing AST not implemented yet");
  }

  void Generator::readMLIR(std::string& filename)
  {
    // Read an MLIR file
    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> srcOrErr =
      llvm::MemoryBuffer::getFileOrSTDIN(filename);

    // Setup source manager and parse
    llvm::SourceMgr sourceMgr;
    sourceMgr.AddNewSourceBuffer(std::move(*srcOrErr), llvm::SMLoc());
    module = mlir::parseSourceFile(sourceMgr, builder.getContext());
  }

  mlir::ModuleOp
  Generator::emitMLIR(llvm::StringRef filename, unsigned optLevel)
  {
    // On error, dump module for debug purposes
    if (mlir::failed(mlir::verify(*module)))
    {
      module->dump();
      throw std::runtime_error("Failed to parse MLIR file");
    }
    if (filename.empty())
      return module.get();

    // Write to the file requested
    std::error_code error;
    auto out = llvm::raw_fd_ostream(filename, error);
    if (error)
      throw std::runtime_error("Failed open output file");

    module->print(out);
    return module.get();
  }

  std::unique_ptr<llvm::Module>
  Generator::emitLLVM(llvm::StringRef filename, unsigned optLevel)
  {
    // The lowering "pass manager"
    mlir::PassManager pm(&context);
    if (optLevel > 0)
    {
      pm.addPass(mlir::createInlinerPass());
      pm.addPass(mlir::createSymbolDCEPass());
      mlir::OpPassManager& optPM = pm.nest<mlir::FuncOp>();
      optPM.addPass(mlir::createCanonicalizerPass());
      optPM.addPass(mlir::createCSEPass());
    }
    pm.addPass(mlir::createLowerToLLVMPass());

    // First lower to LLVM dialect
    if (mlir::failed(pm.run(module.get())))
    {
      module->dump();
      throw std::runtime_error("Failed to lower to LLVM dialect");
    }

    // Then lower to LLVM IR
    auto llvm = mlir::translateModuleToLLVMIR(module.get());
    if (!llvm)
      throw std::runtime_error("Failed to lower to LLVM IR");

    // Write to the file requested
    std::error_code error;
    auto out = llvm::raw_fd_ostream(filename, error);
    if (error)
      throw std::runtime_error("Failed open output file");

    llvm->print(out, nullptr);
    return llvm;
  }
}