# Require EXTRA_SUBDIR_TEST_INCLUDE_FILES be set in the parent scope.
function(set_subdirectory_properties subdir)
    get_filename_component(abs_subdir ${subdir} ABSOLUTE)
    set(GFXSTREAM_THIRD_PARTY_ROOT ${GFXSTREAM_REPO_ROOT})
    cmake_path(APPEND GFXSTREAM_THIRD_PARTY_ROOT "third-party")
    cmake_path(IS_PREFIX GFXSTREAM_THIRD_PARTY_ROOT ${abs_subdir} NORMALIZE IS_THIRD_PARTY)
    if(IS_THIRD_PARTY)
        return()
    endif()
    cmake_path(IS_PREFIX GFXSTREAM_REPO_ROOT ${abs_subdir} NORMALIZE IS_GFXSTREAM_SUBDIR)
    if(NOT IS_GFXSTREAM_SUBDIR)
        return()
    endif()
    cmake_path(IS_PREFIX abs_subdir ${GFXSTREAM_REPO_ROOT} NORMALIZE IS_GFXSTREAM_ROOT)

    get_directory_property(subdirs DIRECTORY "${abs_subdir}" SUBDIRECTORIES)
    foreach(subdir IN LISTS subdirs)
        set_subdirectory_properties("${subdir}")
    endforeach()

    set_property(DIRECTORY "${subdir}" APPEND PROPERTY TEST_INCLUDE_FILES ${EXTRA_SUBDIR_TEST_INCLUDE_FILES})
endfunction()

get_directory_property(subdirs SUBDIRECTORIES)
foreach(subdir IN LISTS subdirs)
    set_subdirectory_properties(${subdir})
endforeach()
