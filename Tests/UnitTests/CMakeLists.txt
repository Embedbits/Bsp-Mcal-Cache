#======================== Unit tests configuration ============================#
# Unit tests of Cache module. Registered in module CMakeLists.txt by
# UnitTesting_AddPath() and created by UnitTesting_Generate() in "UnitTest"
# build (see EmBi_Platform/CMake/UnitTesting/README.md).
#
# The test set is independent - it lists everything it needs: code under test,
# host emulation of the target (registers by RegMem) and libraries built for host.

get_filename_component(CACHE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

UnitTesting_Add_Test(
    NAME            Cache
    TEST_SOURCE     Test_Cache.c
    UUT_SOURCES     ${CACHE_DIR}/Cache.c
                    ${CACHE_DIR}/Cache_Hw.c
    INCLUDE_DIRS    ${CACHE_DIR}
    LINK_LIBS       ut_RegMem                           # Registers in host memory
                    ut_CmsisHost                        # CMSIS core intrinsics on host
    FORCE_INCLUDES  ${UNIT_TESTING_CMSIS_HOST_HEADER}   # Replaces cmsis_gcc.h
    HOST_LIBS       Ral_Lib                             # ST LL compiled for host
    COVERAGE_EXCLUDE ".*/Ral/.*"
)
