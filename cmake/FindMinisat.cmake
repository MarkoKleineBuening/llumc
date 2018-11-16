# Find Minisat
#
#  MINISAT_FOUND       - True if MINISAT was found
#  MINISAT_INCLUDE_DIR - Path to MINISAT's include directory.
#  MINISAT_LIBRARY     - Path to MINISAT's library.

include(FindPackageHandleStandardArgs)

option(LLBMC_SEPARATE_MINISAT "Link in Minisat separately (instead of as part of STP)" OFF)
set(LLBMC_SEPARATE_MINISAT ON)

if(MINISAT_INCLUDE_DIR)
  message("already found")
  # Already found, don't search again
  set(MINISAT_FOUND TRUE)

else (MINISAT_INCLUDE_DIR)

  find_path(MINISAT_INCLUDE_DIR minisat/core/Solver.h DOC "Path to Minisat's include directory")
  mark_as_advanced(
          MINISAT_INCLUDE_DIR
  )

  if (LLBMC_SEPARATE_MINISAT)
    message("does matter")
    find_library(MINISAT_LIBRARY NAMES minisat DOC "Path to Minisat's library")

    FIND_PACKAGE_HANDLE_STANDARD_ARGS(MINISAT DEFAULT_MSG
            MINISAT_LIBRARY
            MINISAT_INCLUDE_DIR
            )

    mark_as_advanced(
            MINISAT_LIBRARY
    )
  endif (LLBMC_SEPARATE_MINISAT)

endif(MINISAT_INCLUDE_DIR)
