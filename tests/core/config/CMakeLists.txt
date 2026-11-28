qzdl_test(core_config Config.cpp FileBrowser.cpp Groups.cpp Profile.cpp Schema.cpp Versions.cpp)
target_compile_definitions(qzdl_test_core_config PRIVATE
        QZDL_CONFIG_FIXTURES="${CMAKE_SOURCE_DIR}/tests/fixtures/config"
        QZDL_SCHEMA_HEADER="${CMAKE_SOURCE_DIR}/src/core/config/Schema.h")
qzdl_test(core_session Session.cpp ConfigPaths.cpp)
qzdl_test(core_import Import.cpp)
