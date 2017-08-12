# Find STP
#
#  STP_FOUND       - True if STP was found
#  STP_INCLUDE_DIR - Path to STP's include directory.
#  STP_LIBRARY     - Path to STP's library.

include(FindPackageHandleStandardArgs)

if(STP_INCLUDE_DIR)

  # Already found, don't search again
  set(STP_FOUND TRUE)

else (STP_INCLUDE_DIR)

  find_path(STP_INCLUDE_DIR stp/c_interface.h DOC "Path to STP's include directory")

  find_library(STP_LIBRARY NAMES stp DOC "Path to STP's library")

  FIND_PACKAGE_HANDLE_STANDARD_ARGS(STP DEFAULT_MSG
    STP_LIBRARY
    STP_INCLUDE_DIR
  )

  mark_as_advanced(
    STP_LIBRARY
    STP_INCLUDE_DIR
  )

endif(STP_INCLUDE_DIR)
