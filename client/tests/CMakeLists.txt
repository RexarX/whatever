# Client Project - Tests
#
# Unit and integration tests using doctest

include(TestUtils)

# doctest should already be available from third_party
# Check if doctest target exists
if(TARGET doctest::doctest)
    set(DOCTEST_TARGET doctest::doctest)
else()
    message(FATAL_ERROR "doctest not found - make sure third_party is added before tests")
endif()

# Create custom targets for organizing tests
add_custom_target(all_tests)
add_custom_target(unit_tests)
add_custom_target(integration_tests)
add_custom_target(core_unit_tests)
add_custom_target(core_integration_tests)
add_custom_target(core_tests)
add_custom_target(comm_unit_tests)
add_custom_target(comm_integration_tests)
add_custom_target(comm_tests)
add_custom_target(runtime_unit_tests)
add_custom_target(runtime_integration_tests)
add_custom_target(runtime_tests)

# Set IDE folders for test targets
client_target_set_folder(all_tests "Client/Tests/Targets")
client_target_set_folder(unit_tests "Client/Tests/Targets")
client_target_set_folder(integration_tests "Client/Tests/Targets")
client_target_set_folder(core_unit_tests "Client/Tests/Targets")
client_target_set_folder(core_integration_tests "Client/Tests/Targets")
client_target_set_folder(core_tests "Client/Tests/Targets")
client_target_set_folder(comm_unit_tests "Client/Tests/Targets")
client_target_set_folder(comm_integration_tests "Client/Tests/Targets")
client_target_set_folder(comm_tests "Client/Tests/Targets")
client_target_set_folder(runtime_unit_tests "Client/Tests/Targets")
client_target_set_folder(runtime_integration_tests "Client/Tests/Targets")
client_target_set_folder(runtime_tests "Client/Tests/Targets")

# Set up aggregation chain: module_type_tests -> type_tests -> all_tests
add_dependencies(all_tests unit_tests integration_tests)
add_dependencies(unit_tests core_unit_tests comm_unit_tests runtime_unit_tests)
add_dependencies(integration_tests core_integration_tests comm_integration_tests runtime_integration_tests)
add_dependencies(core_tests core_unit_tests core_integration_tests)
add_dependencies(comm_tests comm_unit_tests comm_integration_tests)
add_dependencies(runtime_tests runtime_unit_tests runtime_integration_tests)

add_subdirectory(comm)
add_subdirectory(core)
add_subdirectory(runtime)
