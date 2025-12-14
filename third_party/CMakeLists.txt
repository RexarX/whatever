# Third-party dependencies

set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF)

# doctest - Testing framework
add_subdirectory(doctest)

# Set IDE folder
if(TARGET doctest::doctest)
    set_target_properties(doctest PROPERTIES FOLDER "External/Testing")
endif()
