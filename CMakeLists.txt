cmake_minimum_required(VERSION 3.5)
project(llumc)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(LLVM
        REQUIRED COMPONENTS
        LLVMAnalysis
        LLVMIRReader
        LLVMAsmParser
        LLVMBitReader
        LLVMBitWriter
        LLVMCodeGen
        LLVMCore
        LLVMInstCombine
        LLVMInstrumentation
        LLVMLinker
        LLVMMC
        LLVMMCParser
        LLVMScalarOpts
        LLVMSupport
        LLVMTarget
        LLVMTransformUtils
        LLVMVectorize
        LLVMipa
        LLVMipo
        LLVMObjCARCOpts
        LLVMOption
        LLVMProfileData
        LLVMObject
        )
add_definitions("${LLVM_CXXFLAGS} -DLLVM_MAJOR=${LLVM_VERSION_MAJOR} -DLLVM_MINOR=${LLVM_VERSION_MINOR} -fexceptions")
include_directories("${LLVM_INCLUDE_DIR}")
set(SOURCE_FILES main.cpp
        Encoder.cpp Encoder.h)

add_executable(llumc ${SOURCE_FILES})
target_link_libraries(llumc  ${LLVM_LIBRARIES} pthread dl)