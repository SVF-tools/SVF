if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.23)
  project(SVFBoxDomainTests LANGUAGES CXX)
  include(CTest)

  get_filename_component(
    SVF_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE
  )
  list(APPEND CMAKE_MODULE_PATH "${SVF_REPOSITORY_ROOT}/cmake/Modules")
  find_package(GMP REQUIRED)
  find_package(MPFR REQUIRED)

  set(ABSTRACT_DOMAIN_SOURCE_ROOT "${SVF_REPOSITORY_ROOT}/svf")
  add_library(
    AbstractDomainCore
    ${ABSTRACT_DOMAIN_SOURCE_ROOT}/lib/AE/Core/AbstractState.cpp
    ${ABSTRACT_DOMAIN_SOURCE_ROOT}/lib/AE/Core/BoxDomain.cpp
    ${ABSTRACT_DOMAIN_SOURCE_ROOT}/lib/AE/Core/LinearConstraint.cpp
    ${ABSTRACT_DOMAIN_SOURCE_ROOT}/lib/AE/Core/BoxProgramState.cpp
    ${ABSTRACT_DOMAIN_SOURCE_ROOT}/lib/AE/Core/NumericPrimitives.cpp
    ${ABSTRACT_DOMAIN_SOURCE_ROOT}/lib/AE/Core/NumericalDomain.cpp
    ${ABSTRACT_DOMAIN_SOURCE_ROOT}/lib/AE/Core/VariableEnvironment.cpp
  )
  target_compile_features(AbstractDomainCore PUBLIC cxx_std_17)
  target_include_directories(
    AbstractDomainCore PUBLIC ${ABSTRACT_DOMAIN_SOURCE_ROOT}/include
  )
  target_link_libraries(AbstractDomainCore PUBLIC GMP::GMPXX MPFR::MPFR)
endif()

add_executable(svf-box-domain-test BoxDomainTest.cpp)
target_link_libraries(svf-box-domain-test PRIVATE AbstractDomainCore)
add_test(NAME box-domain COMMAND svf-box-domain-test)

if(TARGET SvfLLVM)
  add_executable(svf-box-ae-integration-test BoxAEIntegrationTest.cpp)
  target_link_libraries(svf-box-ae-integration-test PRIVATE SvfLLVM)

  function(add_box_ae_test test_name fixture sparsity)
    add_test(
      NAME ${test_name}
      COMMAND svf-box-ae-integration-test
              -ae-sparsity=${sparsity}
              -widen-delay=1
              -stat=false
              -extapi=${CMAKE_BINARY_DIR}/lib/extapi.bc
              ${CMAKE_CURRENT_LIST_DIR}/${fixture}
    )
  endfunction()

  foreach(sparsity IN ITEMS dense semi-sparse sparse)
    add_box_ae_test(
      box-ae-projection-${sparsity} BoxReducedProduct.ll ${sparsity}
    )
    add_box_ae_test(
      box-ae-loop-${sparsity} BoxLoop.ll ${sparsity}
    )
    add_box_ae_test(
      box-ae-memory-refinement-${sparsity} SparseMemoryFlow.ll ${sparsity}
    )
  endforeach()
  add_box_ae_test(
    box-ae-environment-alignment DenseEnvironmentAlignment.ll dense
  )
  add_box_ae_test(
    box-ae-wide-integer WideIntegerTruncation.ll dense
  )
endif()

if(APPLE AND TARGET z3::libz3)
  get_target_property(Z3_TEST_LIBRARY z3::libz3 IMPORTED_LOCATION)
  get_filename_component(
    Z3_TEST_LIBRARY_DIRECTORY "${Z3_TEST_LIBRARY}" DIRECTORY
  )
  set(box_test_targets svf-box-domain-test)
  if(TARGET svf-box-ae-integration-test)
    list(APPEND box_test_targets svf-box-ae-integration-test)
  endif()
  foreach(box_test_target IN LISTS box_test_targets)
    set_property(
      TARGET ${box_test_target} APPEND
      PROPERTY BUILD_RPATH "${Z3_TEST_LIBRARY_DIRECTORY}"
    )
    add_custom_command(
      TARGET ${box_test_target} POST_BUILD
      COMMAND install_name_tool -change libz3.dylib @rpath/libz3.dylib
              "$<TARGET_FILE:${box_test_target}>"
      VERBATIM
    )
  endforeach()
endif()
