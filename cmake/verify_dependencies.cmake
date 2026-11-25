# Architecture independence checks.
# Fails if a lower layer directly or transitively links a forbidden layer.

function(assert_not_linked target forbidden)
    if(NOT TARGET ${target})
        return()
    endif()
    get_target_property(deps ${target} LINK_LIBRARIES)
    if(NOT deps OR deps STREQUAL "deps-NOTFOUND")
        return()
    endif()
    foreach(dep IN LISTS deps)
        if(dep STREQUAL forbidden)
            message(FATAL_ERROR
                "architecture violation: ${target} links forbidden target ${forbidden}")
        endif()
        # Do not recurse into imported/system libraries.
        if(TARGET ${dep})
            assert_not_linked(${dep} ${forbidden})
        endif()
    endforeach()
endfunction()

assert_not_linked(mpl_core mpl_knowledge)
assert_not_linked(mpl_core mpl_experiment)
assert_not_linked(mpl_core winhttp)
assert_not_linked(mpl_knowledge mpl_experiment)
assert_not_linked(mpl_knowledge winhttp)
assert_not_linked(mpl_demo mpl_experiment)
assert_not_linked(mpl_demo winhttp)
assert_not_linked(mplc mpl_knowledge)
assert_not_linked(mplc mpl_experiment)
assert_not_linked(mplc winhttp)

if(TARGET mpl_experiment)
    get_target_property(experiment_deps mpl_experiment LINK_LIBRARIES)
    if(NOT experiment_deps MATCHES "winhttp")
        message(FATAL_ERROR "mpl_experiment should be the only layer linking winhttp")
    endif()
endif()

message(STATUS "architecture independence: pass")
