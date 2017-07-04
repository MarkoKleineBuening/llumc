# Find terminfo library
#
#  LIBTERMINFO_FOUND       - True if terminfo library was found.
#  LIBTERMINFO_LIBRARY     - Path to terminfo library.

include(FindPackageHandleStandardArgs)

if(LIBTERMINFO_LIBRARY)

  # Already found, don't search again
  set(LIBTERMINFO_FOUND TRUE)

else(LIBTERMINFO_LIBRARY)

  find_library(LIBTERMINFO_LIBRARY NAMES tinfo libtinfo curses libcurses ncurses libncurses ncursesw libncursesw DOC "Path to terminfo library")

  FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibTermInfo DEFAULT_MSG
    LIBTERMINFO_LIBRARY
  )

  mark_as_advanced(
    LIBTERMINFO_LIBRARY
  )

endif(LIBTERMINFO_LIBRARY)
