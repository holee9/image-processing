# Compiler warnings configuration

if(MSVC)
    # /W4 and /utf-8 applied globally. /WX is NOT set globally: a global /WX
    # combined with a per-target /WX- produces D9025 on every TU without changing
    # effective behaviour (the later per-target flag wins regardless).
    #
    # Warnings-as-errors is therefore delegated to individual targets via
    # xpe_target_warnings_as_errors() below. Call it from a module CMakeLists
    # once that target builds clean; until then leave the existing /WX-.
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(XPE_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
endif()

# Applies warnings-as-errors to one target, honouring XPE_WARNINGS_AS_ERRORS.
#
# Replaces a per-target /WX- with an opt-in that the option actually controls.
# Before this helper existed the option was inert on MSVC: the comment above
# delegated /WX to the modules, but no module implemented the delegation, so
# XPE_WARNINGS_AS_ERRORS=ON changed nothing (see issue #106).
#
# Usage — replace
#     target_compile_options(my_target PRIVATE /W4 /WX-)
# with
#     xpe_target_warnings_as_errors(my_target)
# only after confirming the target builds with zero warnings. A target that
# still emits warnings must keep its /WX- until they are resolved.
function(xpe_target_warnings_as_errors target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "xpe_target_warnings_as_errors: no such target '${target}'")
    endif()
    if(NOT XPE_WARNINGS_AS_ERRORS)
        return()
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /WX)
    else()
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()
