# Find LLBMC
#
#  LLBMC_FOUND       - True if LLBMC was found
#  LLBMC_INCLUDE_DIR - Path to LLBMC's include directory.
#  LLBMC_LIBRARY     - Path to LLBMC's library.

include(FindPackageHandleStandardArgs)
include(FindComponents)

if (LLBMC_LIBRARIES)

  # Already found, don't search again
  set(LLBMC_FOUND TRUE)

else ()

  find_path(LLBMC_INCLUDE_DIR llbmc/Tools/FunctionChecker.h DOC "Path to LLBMC's include directory")

  set(LLBMC_COMPONENTS
          CommandLine
          Tools
          Output
          CallGraph
          Passes
          Encoder
          Analysis
          Transformer
          Solver
          ILR
          SMT
          Solver
          Resource
          Util
          )

  find_components(LLBMC ${LLBMC_FIND_COMPONENTS})

  set(LLBMC_LIBRARIES ${LLBMC_LIBRARIES} CACHE STRING "List of all requested llbmc libraries")

  FIND_PACKAGE_HANDLE_STANDARD_ARGS(LLBMC DEFAULT_MSG
    LLBMC_INCLUDE_DIR
    ${LLBMC_LIBRARY_VARIABLES}
  )

  mark_as_advanced(
    LLBMC_INCLUDE_DIR
    ${LLBMC_LIBRARY_VARIABLES}
  )

endif()
