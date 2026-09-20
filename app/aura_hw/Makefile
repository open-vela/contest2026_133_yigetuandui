############################################################################
# aura_hw/Makefile
# 集成前提：本目录整体放入 openvela 的 apps/ 下（即 $(APPDIR)/aura_hw），
# 并在 apps/Kconfig 中追加：source "$APPSDIR/aura_hw/Kconfig"
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = aura
PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = $(CONFIG_EXAMPLES_AURA_HW_STACKSIZE)
MODULE    = $(CONFIG_EXAMPLES_AURA_HW)

MAINSRC  = aura_main.c

CSRCS    = event_bus.c lsm6ds3.c imu_service.c gesture_recognizer.c
CSRCS   += vib_motor.c radar_store.c glicko2.c game_port.c

# 复用后端（Aura_Space_Backend）的可移植 C 游戏引擎，同步副本见 engine/
CSRCS   += engine/beatmap_parser.c engine/judge_system.c engine/cJSON.c

CFLAGS += -I$(APPDIR)/aura_hw/engine

include $(APPDIR)/Application.mk
