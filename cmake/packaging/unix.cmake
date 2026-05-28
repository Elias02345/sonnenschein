# unix specific packaging
# put anything here that applies to both linux and macos

# return here if building a macos package
if(SUNSHINE_PACKAGE_MACOS)
    return()
endif()

# Installation destination dir
set(CPACK_SET_DESTDIR true)
if(NOT CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX "/usr/share/sunshine")
endif()

install(TARGETS sunshine RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# Backwards-compatibility symlink: the binary ships as `sonnenschein`, but older
# scripts, services and muscle memory still invoke `sunshine`.
install(CODE "
    set(_bindir \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
    execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink
            sonnenschein \"\${_bindir}/sunshine\")
")
