macro(find_components PACKAGE)
  string(TOUPPER ${PACKAGE} PACKAGE_UPPER)

  # check spelling of components
  foreach (REQUESTED_COMPONENT ${ARGN})
    set(FOUND FALSE)
    foreach (KNOWN_COMPONENT ${${PACKAGE_UPPER}_COMPONENTS})
      if (REQUESTED_COMPONENT STREQUAL ${PACKAGE}${KNOWN_COMPONENT})
        set(FOUND TRUE)
      endif ()
    endforeach ()
    if (NOT FOUND)
      message (FATAL_ERROR "${REQUESTED_COMPONENT} is no known component of ${PACKAGE}")
    endif ()
  endforeach ()

  # ordering the loops this way ensure the libraries are listed in the right order
  foreach (KNOWN_COMPONENT ${${PACKAGE_UPPER}_COMPONENTS})
    foreach (REQUESTED_COMPONENT ${ARGN})
      if (REQUESTED_COMPONENT STREQUAL ${PACKAGE}${KNOWN_COMPONENT})
        set(COMPONENT_NAME ${KNOWN_COMPONENT})

        # create the name for the variable
        string (TOUPPER ${COMPONENT_NAME} COMPONENT_VARIABLE)
        set(COMPONENT_VARIABLE "${PACKAGE_UPPER}_${COMPONENT_VARIABLE}_LIBRARY")

        # note that we expect a certain library naming pattern, e.g.
        # LLVMAnalysis instead of Analysis
        find_library (${COMPONENT_VARIABLE} ${PACKAGE}${COMPONENT_NAME}
                      HINTS ${LLVM_LIBRARY_PATH_HINT}
        )

        list (APPEND ${PACKAGE_UPPER}_COMPONENT_VARIABLES ${COMPONENT_VARIABLE})
        list (APPEND ${PACKAGE_UPPER}_LIBRARIES ${${COMPONENT_VARIABLE}})

      endif ()
    endforeach ()
  endforeach ()

endmacro()


