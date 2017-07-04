include(FindPackageHandleStandardArgs)
include(FindPackageMessage)
include(FindComponents)

if (LLVM_LIBRARIES)

  set(LLVM_FOUND TRUE)

else (LLVM_LIBRARIES)

  # try to find llvm-config
  #########################

  # try to find 'llvm-config' executable
  find_program (LLVM_CONFIG_EXECUTABLE "llvm-config" HINTS ${LLVM_BINARY_PATH_HINT} DOC "Path to the llvm-config executable")

  # if available, use llvm-config to provide hints later on
  if (LLVM_CONFIG_EXECUTABLE)

    # get a hint about the library path from 'llvm-config'
    execute_process (
      COMMAND
        ${LLVM_CONFIG_EXECUTABLE} --libdir
      OUTPUT_VARIABLE
        LLVM_LIBRARY_PATH_HINT
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # get a hint about the binary path from 'llvm-config'
    execute_process (
      COMMAND
        ${LLVM_CONFIG_EXECUTABLE} --bindir
      OUTPUT_VARIABLE
        LLVM_BINARY_PATH_HINT
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # get a hint about the include path from 'llvm-config'
    execute_process (
      COMMAND
        ${LLVM_CONFIG_EXECUTABLE} --includedir
      OUTPUT_VARIABLE
        LLVM_INCLUDE_PATH_HINT
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(LLVM_SOME_EXECUTABLE ${LLVM_CONFIG_EXECUTABLE})

  endif (LLVM_CONFIG_EXECUTABLE)

  # find executables
  ##################

  # add more executables to this list as required
  set (LLVM_EXECUTABLE_NAMES
    llvm-as
    llvm-dis
    llvm-link
  )

  foreach(EXECUTABLE_NAME ${LLVM_EXECUTABLE_NAMES})

    string(TOUPPER ${EXECUTABLE_NAME} EXECUTABLE_VARIABLE)
    string(REPLACE "-" "_" EXECUTABLE_VARIABLE ${EXECUTABLE_VARIABLE})
    set(EXECUTABLE_VARIABLE "${EXECUTABLE_VARIABLE}_EXECUTABLE")

    find_program(${EXECUTABLE_VARIABLE} ${EXECUTABLE_NAME} HINTS ${LLVM_BINARY_PATH_HINT})
    list(APPEND LLVM_EXECUTABLE_VARIABLES ${EXECUTABLE_VARIABLE})

    # set the first executable we can find (we use that one for the version extraction)
    if (NOT LLVM_SOME_EXECUTABLE)
      set(LLVM_SOME_EXECUTABLE ${${EXECUTABLE_VARIABLE}})
    endif (NOT LLVM_SOME_EXECUTABLE)

  endforeach(EXECUTABLE_NAME ${LLVM_EXECUTABLE_NAMES})


  # version string
  ################

  if(LLVM_SOME_EXECUTABLE)
    execute_process (
      COMMAND
        ${LLVM_SOME_EXECUTABLE} --version
      OUTPUT_VARIABLE
        LLVM_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  else (LLVM_SOME_EXECUTABLE)
    message(FATAL_ERROR "No LLVM executable found")
  endif(LLVM_SOME_EXECUTABLE)

  string(COMPARE EQUAL "${LLVM_VERSION}" "" EMPTY_VERSION)
  if (EMPTY_VERSION)
    message(FATAL_ERROR "Got empty LLVM version")
  endif (EMPTY_VERSION)

  string(REGEX REPLACE "([0-9]+)\\.[0-9]+(\\.[0-9]+)?(svn)?" "\\1" LLVM_VERSION_MAJOR "${LLVM_VERSION}")
  set(LLVM_VERSION_MAJOR ${LLVM_VERSION_MAJOR} CACHE STRING "LLVM's major version number")
  string(REGEX REPLACE "[0-9]+\\.([0-9]+)(\\.[0-9]+)?(svn)?" "\\1" LLVM_VERSION_MINOR "${LLVM_VERSION}")
  set(LLVM_VERSION_MINOR ${LLVM_VERSION_MINOR} CACHE STRING "LLVM's minor version number")

  if (${LLVM_VERSION} MATCHES "[0-9]+\\.[0-9]+\\.[0-9]+(svn)?")
    string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+)(svn)?" "\\1" LLVM_VERSION_PATCHLEVEL "${LLVM_VERSION}")
    set(LLVM_VERSION_PATCHLEVEL ${LLVM_VERSION_PATCHLEVEL} CACHE STRING "LLVM'S patch level")
  else()
    set(LLVM_VERSION_PATCHLEVEL "0" CACHE STRING "LLVM's patch level")
  endif()

  set(LLVM_VERSION ${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR} CACHE STRING "LLVM's version number without the patch level")

  find_package_message (LLVM "LLVM version: ${LLVM_VERSION}" "${LLVM_VERSION}")


  # find include dir
  ##################

  find_path (LLVM_INCLUDE_DIR "llvm/Pass.h" HINTS ${LLVM_INCLUDE_PATH_HINT} DOC "")


  # filter requested components for older llvm releases
  #####################################################

  # we silently filter out some requested components for older llvm version,
  # when does components did not yet exist
  set(REQUESTED_COMPONENTS)
  foreach (REQUESTED_COMPONENT ${LLVM_FIND_COMPONENTS})
    set(SKIP_COMPONENT FALSE)

    if (LLVM_VERSION VERSION_LESS "3.1" AND REQUESTED_COMPONENT STREQUAL "LLVMVectorize")
      set (SKIP_COMPONENT TRUE)
    endif ()

    if (LLVM_VERSION VERSION_LESS "3.3" AND REQUESTED_COMPONENT STREQUAL "LLVMObjCARCOpts")
      set (SKIP_COMPONENT TRUE)
    endif ()

    if (LLVM_VERSION VERSION_LESS "3.3" AND REQUESTED_COMPONENT STREQUAL "LLVMIRReader")
      set (SKIP_COMPONENT TRUE)
    endif ()

    if (LLVM_VERSION VERSION_LESS "3.4" AND REQUESTED_COMPONENT STREQUAL "LLVMOption")
      set (SKIP_COMPONENT TRUE)
    endif ()

    if (LLVM_VERSION VERSION_LESS "3.5" AND REQUESTED_COMPONENT STREQUAL "LLVMProfileData")
      set (SKIP_COMPONENT TRUE)
    endif ()

    if (LLVM_VERSION VERSION_LESS "3.6" AND REQUESTED_COMPONENT STREQUAL "LLVMObject")
      set (SKIP_COMPONENT TRUE)
    endif ()

    # add to list only if not skipped
    if (NOT SKIP_COMPONENT)
      list(APPEND REQUESTED_COMPONENTS ${REQUESTED_COMPONENT})
    endif ()

  endforeach ()

  # find libraries
  ################

  # this list defines the library linking order, do not sort it, unless you
  # want to change the order in which these libraries are linked to.
  set (LLVM_COMPONENTS
    "Object"
    "ObjCARCOpts"
    "X86AsmParser"
    "X86AsmPrinter"
    "X86CodeGen"
    "X86Info"
    "MCParser"
    "MC"
    "Interpreter"
    "ExecutionEngine"
    "SelectionDAG"
    "Instrumentation"
    "CodeGen"
    "ipo"
    "Vectorize"
    "InstCombine"
    "ScalarOpts"
    "TransformUtils"
    "ipa"
    "Analysis"
    "Target"
    "Linker"
    "Archive"
    "IRReader"
    "AsmParser"
    "AsmPrinter"
    "Debugger"
    "BitReader"
    "BitWriter"
    "ProfileData"
    "Core"
    "Option"
    "Support"
  )

  # find the requested components, store them in LLVM_${COMPONENT}_LIBRARY
  # also fill LLVM_COMPONENTS with a list of the components, in the right order
  # finally, also store the names of the variables in LLVM_COMPONENT_VARIABLES
  # for use in the fphsa macro
  find_components(LLVM ${LLVM_FIND_COMPONENTS})

  # copy LLVM_COMPONENTS over to the cache as LLVM_LIBRARIES
  set(LLVM_LIBRARIES ${LLVM_LIBRARIES} CACHE STRING "List of all requested llvm libraries")


  # cxx flags
  ###########

  if (LLVM_CONFIG_EXECUTABLE)

    # retrieve cxx flags for compiling llvm
    execute_process(
      COMMAND
        ${LLVM_CONFIG_EXECUTABLE} --cxxflags
      OUTPUT_VARIABLE
        LLVM_CXXFLAGS
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # too long, don't want to print this:
    #find_package_message(LLVM "Using LLVM's CXXFlags: ${LLVM_CXXFLAGS}" "${LLVM_CXXFLAGS}")

  else (LLVM_CONFIG_EXECUTABLE)
    set (LLVM_CXXFLAGS "-fnoexception")
  endif (LLVM_CONFIG_EXECUTABLE)

  set(LLVM_CXXFLAGS ${LLVM_CXXFLAGS} CACHE STRING "llvm's CXXFLAGS")

  # make sure we got everything
  #############################

  # check that everything is present; abort if not
  find_package_handle_standard_args (LLVM DEFAULT_MSG
    LLVM_INCLUDE_DIR
    LLVM_VERSION
    LLVM_LIBRARIES
    LLVM_CXXFLAGS
    ${LLVM_EXECUTABLE_VARIABLES}
    ${LLVM_COMPONENT_VARIABLES}
  )

  mark_as_advanced(
    LLVM_INCLUDE_DIR
    LLVM_VERSION
    LLVM_VERSION_MAJOR
    LLVM_VERSION_MINOR
    LLVM_VERSION_PATCHLEVEL
    LLVM_LIBRARIES
    LLVM_CXXFLAGS
    ${LLVM_EXECUTABLE_VARIABLES}
    ${LLVM_COMPONENT_VARIABLES}
  )

endif (LLVM_LIBRARIES)
