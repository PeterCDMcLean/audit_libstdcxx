add_executable(python_audit_libstdcxx IMPORTED GLOBAL)
set_target_properties(python_audit_libstdcxx PROPERTIES
  IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/audit_libstdcxx.py
)

add_executable(python_sitecustomize_template IMPORTED GLOBAL)
set_target_properties(python_sitecustomize_template PROPERTIES
  IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/sitecustomize.py.in
)
