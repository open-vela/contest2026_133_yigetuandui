############################################################################
# apps/aura_hw/CMakeLists.txt
# Aura-Space 黄山派板端服务（openvela cmake 构建入口）
#
# 注意：openvela cmake 构建通过 apps/CMakeLists.txt 的 add_subdirectory()
# 发现本应用，Kconfig 符号也经由该遍历收集（根 apps/Kconfig 不参与
# cmake 构建）。因此本文件必须存在，且 apps/CMakeLists.txt 需包含：
#   add_subdirectory(aura_hw)
############################################################################

if(CONFIG_EXAMPLES_AURA_HW)

  set(AURA_SRCS
      aura_main.c
      aura_app.c
      aura_ui.c
      music.c
      nback.c
      focus.c
      coach.c
      event_bus.c
      lsm6ds3.c
      imu_service.c
      gesture_recognizer.c
      vib_motor.c
      radar_store.c
      glicko2.c
      game_port.c
      engine/beatmap_parser.c
      engine/judge_system.c
      engine/cJSON.c)

  nuttx_add_application(
    NAME
    aura
    PRIORITY
    SCHED_PRIORITY_DEFAULT
    STACKSIZE
    ${CONFIG_EXAMPLES_AURA_HW_STACKSIZE}
    MODULE
    ${CONFIG_EXAMPLES_AURA_HW}
    INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_LIST_DIR}/engine
    SRCS
    ${AURA_SRCS})

endif()
