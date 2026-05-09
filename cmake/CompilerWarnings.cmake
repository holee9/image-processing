# Compiler warnings configuration

if(MSVC)
    # /W4 and /utf-8 applied globally. /WX is NOT set globally because every
    # module CMakeLists already applies /WX- (or /WX) per-target; a global /WX
    # combined with per-target /WX- produces D9025 noise on every TU without
    # changing effective behaviour (the later per-target flag wins regardless).
    # Modules that require warnings-as-errors set /WX themselves via
    # if(XPE_WARNINGS_AS_ERRORS) in their own CMakeLists.txt.
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(XPE_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
endif()
