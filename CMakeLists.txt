cmake_minimum_required(VERSION 3.5)
project(llumc)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

#add_definitions(-fsanitize=address)
#add_definitions(-lasan)
#set(CMAKE_CXX_LINK_FLAGS ${CMAKE_CXX_LINK_FLAGS} -fsanitize=address)

#set(COMMON_DEFINITIONS "${COMMON_DEFINITIONS} -static")
set (CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -static")

find_package(LLBMC
        COMPONENTS
        LLBMCAnalysis
        LLBMCCallGraph
        LLBMCCommandLine
        LLBMCEncoder
        LLBMCOutput
        LLBMCPasses
        LLBMCResource
        LLBMCSMT
        LLBMCSolver
        LLBMCTools
        LLBMCILR
        LLBMCTransformer
        LLBMCUtil
        )

add_definitions(${LLBMC_DEFINITIONS} -fno-rtti)
include_directories(${LLBMC_INCLUDE_DIR})

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

# optionally find STP, enabled by default
option(LLBMC_ENABLE_STP "Enable LLBMC's STP support" ON)
if (LLBMC_ENABLE_STP)
    find_package(STP)
    if (STP_FOUND)
        set (LLBMC_SMT_DEFINITIONS "${LLBMC_SMT_DEFINITIONS} -DWITH_STP")
        find_package(Minisat)
    endif (STP_FOUND)
endif (LLBMC_ENABLE_STP)

set(SOURCE_FILES main.cpp
        Encoder.cpp Encoder.h InstructionEncoderLLUMC.cpp InstructionEncoderLLUMC.h SMTTranslator.cpp SMTTranslator.h)

message(STATUS "Final COMMON_DEFINITIONS flags ${COMMON_DEFINITIONS}")
message(STATUS "Final CMAKE_CXX_LINK_EXECUTABLE flags ${CMAKE_CXX_LINK_EXECUTABLE}")

add_executable(llumc ${SOURCE_FILES})

target_link_libraries(llumc  ${LLBMC_LIBRARIES} pthread dl)

target_link_libraries(llumc  ${LLVM_LIBRARIES} pthread dl)

target_link_libraries(llumc ${STP_LIBRARY})
message(STP_Libary: ${STP_LIBRARY})
message(Minisat: ${MINISAT_LIBRARY})
target_link_libraries(llumc ${MINISAT_LIBRARY})

