cmake_minimum_required(VERSION 3.16)
project(LanLinkSlim VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT DEFINED TDE2E_PREFIX)
  set(TDE2E_PREFIX /workspace/deps)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)

add_library(lanlink_core
  lan/identity.cpp
  lan/protocol.cpp
  lan/node.cpp
  lan/e2e.cpp
  lan/transfer.cpp
  lan/group.cpp
)
target_include_directories(lanlink_core PUBLIC
  ${CMAKE_SOURCE_DIR}/lan
  ${TDE2E_PREFIX}/include
)
target_link_directories(lanlink_core PUBLIC ${TDE2E_PREFIX}/lib)
target_link_libraries(lanlink_core PUBLIC tde2e tdutils crypto z)
if(WIN32)
  target_compile_definitions(lanlink_core PUBLIC _WIN32_WINNT=0x0A00)
  target_link_libraries(lanlink_core PUBLIC ws2_32 bcrypt crypt32)
else()
  target_link_libraries(lanlink_core PUBLIC pthread dl)
endif()

add_executable(LanLink
  app/main.cpp
  app/markdown.cpp
  app/history_text.cpp
  app/theme.cpp
  app/i18n.cpp
  app/app_settings.cpp
  app/popup_menu.cpp
  app/settings_box.cpp
  app/working_dir.cpp
  app/history_store.cpp
  app/dialog_merge.cpp
  app/file_bubble.cpp
  app/avatar.cpp
  app/icons.cpp
  app/emoji_catalog.cpp
  app/emoji_catalog_data.cpp
  app/emoji_panel.cpp
  app/node_controller.cpp
  app/dialog_list.cpp
  app/compose_bar.cpp
  app/chat_view.cpp
  app/new_group_box.cpp
  app/profile_box.cpp
  app/members_box.cpp
  app/official_box.cpp
  app/main_menu.cpp
  app/main_window.cpp
  resources/lanlink.qrc
)
target_include_directories(LanLink PRIVATE ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/lan)
target_link_libraries(LanLink PRIVATE lanlink_core Qt6::Core Qt6::Gui Qt6::Widgets)

if(MINGW)
  # Console-less Windows GUI with an embedded .ico.
  enable_language(RC)
  target_sources(LanLink PRIVATE resources/lanlink.rc)
  set_target_properties(LanLink PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

add_executable(lan_tests
  lan/tests/test_main.cpp
  lan/tests/test_identity.cpp
  lan/tests/test_protocol.cpp
  lan/tests/test_node.cpp
  lan/tests/test_e2e.cpp
  lan/tests/test_transfer.cpp
  lan/tests/test_group.cpp
  lan/tests/test_markdown.cpp
  lan/tests/test_i18n.cpp
  lan/tests/test_theme.cpp
  lan/tests/test_history_text.cpp
  lan/tests/test_compose_dialogs.cpp
  lan/tests/test_dialog_merge.cpp
  lan/tests/test_step_policy.cpp
  lan/tests/test_node_controller.cpp
  app/node_controller.cpp
  app/avatar.cpp
  app/emoji_catalog.cpp
  app/emoji_catalog_data.cpp
  app/markdown.cpp
  app/history_text.cpp
  app/history_store.cpp
  app/dialog_merge.cpp
  app/file_bubble.cpp
  app/i18n.cpp
  app/theme.cpp
)
target_include_directories(lan_tests PRIVATE ${CMAKE_SOURCE_DIR}/lan ${CMAKE_SOURCE_DIR}/lan/tests ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(lan_tests PRIVATE lanlink_core Qt6::Core Qt6::Gui Qt6::Widgets)

add_executable(lan_node
  lan/lan_cli.cpp
)
target_link_libraries(lan_node PRIVATE lanlink_core)
