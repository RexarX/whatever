# Unit Tests

# Example unit test
# embedded_add_unit_test(
#     NAME example_unit_test
#     SOURCES
#         example_test.cpp
#     LIBRARIES
#         # Add any libraries needed for testing
#     INCLUDE_DIRS
#         ${EMBEDDED_ROOT_DIR}/main
#         ${EMBEDDED_ROOT_DIR}/include
#     MODULE core
# )

# Note: Add your unit tests here using the embedded_add_unit_test() function
# Example structure:
#
# embedded_add_unit_test(
#     NAME servo_controller_test
#     SOURCES
#         servo_controller_test.cpp
#         ${EMBEDDED_ROOT_DIR}/main/servo_controller.cpp  # Source under test
#     INCLUDE_DIRS
#         ${EMBEDDED_ROOT_DIR}/main
#     MODULE servo
# )

message(STATUS "Unit tests directory configured (add tests in this CMakeLists.txt)")
