##
## Auto Generated makefile by CDK
## Do not modify this file, and any manual changes will be erased!!!   
##
## psdf
ProjectName            :=radar_analysis_sleep_MRS6130_P1812
ConfigurationName      :=psdf
WorkspacePath          :=../radar_sleep_llx/
ProjectPath            :=./
IntermediateDirectory  :=Obj
OutDir                 :=$(IntermediateDirectory)
User                   :=llx
Date                   :=25/06/2026
CDKPath                :=../../../../../../../../../C-Sky/CDK
ToolchainPath          :=D:/C-Sky/CDKRepo/Toolchain/XTGccElfNewlib/V3.1.0/R/
LinkerName             :=riscv64-unknown-elf-gcc
LinkerNameoption       :=
SIZE                   :=riscv64-unknown-elf-size
READELF                :=riscv64-unknown-elf-readelf
CHECKSUM               :=crc32
SharedObjectLinkerName :=
ObjectSuffix           :=.o
DependSuffix           :=.d
PreprocessSuffix       :=.i
DisassemSuffix         :=.asm
IHexSuffix             :=.ihex
BinSuffix              :=.bin
ExeSuffix              :=.elf
LibSuffix              :=.a
DebugSwitch            :=-g 
IncludeSwitch          :=-I
LibrarySwitch          :=-l
OutputSwitch           :=-o 
ElfInfoSwitch          :=-hlS
LibraryPathSwitch      :=-L
PreprocessorSwitch     :=-D
UnPreprocessorSwitch   :=-U
SourceSwitch           :=-c 
ObjdumpSwitch          :=-S
ObjcopySwitch          :=-O ihex
ObjcopyBinSwitch       :=-O binary
OutputFile             :=psdf
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E
PreprocessOnlyDisableLineSwitch   :=-P
ObjectsFileList        :=$(IntermediateDirectory)/radar_analysis_sleep_MRS6130_P1812.txt
MakeDirCommand         :=mkdir
LinkOptions            := -mcpu=e906f  -nostartfiles -Wl,--gc-sections -T"$(ProjectPath)/../../../../platform/utils/linker_cpus.ld" -pipe 
LinkOtherFlagsOption   :=-MP -MMD -Os -Wno-main -mcmodel=medlow  $(ProjectPath)../../../../libs/mmw/mmw_ctrl/rs6130/mmw_ctrl.sym $(ProjectPath)../../../../libs/bootloader/psdfboot.sym -Wl,-Map=$(ProjectPath)/Lst/$(OutputFile).map 
IncludePackagePath     :=
IncludeCPath           := $(IncludeSwitch). $(IncludeSwitch)../../../../kernel/freeRTOS/Source/include $(IncludeSwitch)../../../../kernel/freeRTOS/Source/portable $(IncludeSwitch)../../../../kernel/freeRTOS/inc $(IncludeSwitch)../../../../kernel/freeRTOS/portable $(IncludeSwitch)../../../../kernel/inc $(IncludeSwitch)../../../../platform/arch/riscv/inc $(IncludeSwitch)../../../../platform/boards $(IncludeSwitch)../../../../platform/boards/mrs6130_p1806 $(IncludeSwitch)../../../../platform/boards/mrs6130_p1812 $(IncludeSwitch)../../../../platform/boards/mrs6240_p2512 $(IncludeSwitch)../../../../platform/boards/mrs6242_p2512 $(IncludeSwitch)../../../../platform/drivers/inc $(IncludeSwitch)../../../../platform/drivers/inc/hw $(IncludeSwitch)../../../../platform/drivers/inc/ll $(IncludeSwitch)../../../../platform/soc/common/inc $(IncludeSwitch)../../../../platform/soc/rs6130/inc $(IncludeSwitch)../../../../platform/soc/rs613x/inc $(IncludeSwitch)../../../../platform/soc/rs6240/inc $(IncludeSwitch)../../../../platform/soc/rs624x/inc $(IncludeSwitch)../../../../platform/utils $(IncludeSwitch)../../../../subsys/cmd $(IncludeSwitch)../../../../subsys/dsp/inc $(IncludeSwitch)../../../../subsys/ft/inc $(IncludeSwitch)../../../../subsys/hif/inc $(IncludeSwitch)../../../../subsys/kv/kvf/inc $(IncludeSwitch)../../../../subsys/kv/kvr/inc $(IncludeSwitch)../../../../subsys/libc/compilers/gcc $(IncludeSwitch)../../../../subsys/libc/compilers/gcc/sys $(IncludeSwitch)../../../../subsys/libc/include $(IncludeSwitch)../../../../subsys/libc/include/serf $(IncludeSwitch)../../../../subsys/libc/include/sys $(IncludeSwitch)../../../../subsys/log/inc $(IncludeSwitch)../../../../subsys/mmw $(IncludeSwitch)../../../../subsys/mmw/mmw_algorithm $(IncludeSwitch)../../../../subsys/mmw/mmw_algorithm/inc $(IncludeSwitch)../../../../subsys/mmw/mmw_application $(IncludeSwitch)../../../../subsys/mmw/mmw_application/inc $(IncludeSwitch)../../../../subsys/mmw/mmw_application/test $(IncludeSwitch)../../../../subsys/mmw/mmw_cmd/inc $(IncludeSwitch)../../../../subsys/mmw/mmw_cmd_sleep/inc $(IncludeSwitch)../../../../subsys/mmw/mmw_ctrl/inc $(IncludeSwitch)../../../../subsys/mmw/mmw_mdsp $(IncludeSwitch)../../../../subsys/mmw/mmw_mimo $(IncludeSwitch)../../../../subsys/mmw/mmw_point_cloud $(IncludeSwitch)../../../../subsys/mmw/mmw_point_cloud/mmw_process $(IncludeSwitch)../../../../subsys/mmw/mmw_point_cloud/mmw_process/common_lib $(IncludeSwitch)../../../../subsys/mmw/mmw_point_cloud/mmw_process/mmw_psic_lib $(IncludeSwitch)../../../../subsys/mmw/mmw_point_cloud/mmw_process/mmw_psic_lib/cluster $(IncludeSwitch)../../../../subsys/mmw/mmw_point_cloud/test $(IncludeSwitch)../../../../subsys/mmw/mmw_psic_lib $(IncludeSwitch)../../../../subsys/mmw/mmw_psic_lib/cluster $(IncludeSwitch)../../../../subsys/ota/inc $(IncludeSwitch)../../../../subsys/pm/include $(IncludeSwitch)../../../../subsys/pm/port $(IncludeSwitch)../../../../subsys/pm/sys $(IncludeSwitch)../../../../subsys/shell/letter_shell/inc $(IncludeSwitch)../../../../subsys/test $(IncludeSwitch)../../../../subsys/testsuite/inc $(IncludeSwitch)../app/inc  
IncludeAPath           := $(IncludeSwitch). $(IncludeSwitch)../../../../kernel/freeRTOS/Source/include $(IncludeSwitch)../../../../kernel/freeRTOS/Source/portable $(IncludeSwitch)../../../../kernel/freeRTOS/inc $(IncludeSwitch)../../../../kernel/freeRTOS/portable $(IncludeSwitch)../../../../kernel/inc $(IncludeSwitch)../../../../platform/arch/riscv/inc $(IncludeSwitch)../../../../platform/boards $(IncludeSwitch)../../../../platform/boards/mrs6130_p1806 $(IncludeSwitch)../../../../platform/boards/mrs6130_p1812 $(IncludeSwitch)../../../../platform/boards/mrs6240_p2512 $(IncludeSwitch)../../../../platform/boards/mrs6242_p2512 $(IncludeSwitch)../../../../platform/drivers/inc $(IncludeSwitch)../../../../platform/drivers/inc/hw $(IncludeSwitch)../../../../platform/drivers/inc/ll $(IncludeSwitch)../../../../platform/soc/common/inc $(IncludeSwitch)../../../../platform/soc/rs6130/inc $(IncludeSwitch)../../../../platform/soc/rs613x/inc $(IncludeSwitch)../../../../platform/soc/rs6240/inc $(IncludeSwitch)../../../../platform/soc/rs624x/inc $(IncludeSwitch)../../../../platform/utils $(IncludeSwitch)../../../../subsys/cmd $(IncludeSwitch)../../../../subsys/dsp/inc $(IncludeSwitch)../../../../subsys/ft/inc $(IncludeSwitch)../../../../subsys/hif/inc $(IncludeSwitch)../../../../subsys/kv/kvf/inc $(IncludeSwitch)../../../../subsys/kv/kvr/inc $(IncludeSwitch)../../../../subsys/libc/compilers/gcc $(IncludeSwitch)../../../../subsys/libc/compilers/gcc/sys $(IncludeSwitch)../../../../subsys/libc/include $(IncludeSwitch)../../../../subsys/libc/include/serf $(IncludeSwitch)../../../../subsys/libc/include/sys $(IncludeSwitch)../../../../subsys/log/inc $(IncludeSwitch)../../../../subsys/mmw $(IncludeSwitch)../../../../subsys/mmw/mmw_algorithm $(IncludeSwitch)../../../../subsys/mmw/mmw_algorithm/inc $(IncludeSwitch)../../../../subsys/mmw/mmw_application $(IncludeSwitch)../../../../subsys/mmw/mmw_application/inc $(IncludeSwitch)../../../../subsys/mmw/mmw_application/test $(IncludeSwitch)../../../../subsys/mmw/mmw_cmd/inc $(IncludeSwitch)../../../../subsys/mmw/mmw_cmd_sleep/inc $(IncludeSwitch)../../../../subsys/mmw/mmw_ctrl/inc $(IncludeSwitch)../../../../subsys/mmw/mmw_mdsp $(IncludeSwitch)../../../../subsys/mmw/mmw_mimo $(IncludeSwitch)../../../../subsys/mmw/mmw_point_cloud $(IncludeSwitch)../../../../subsys/mmw/mmw_point_cloud/mmw_process $(IncludeSwitch)../../../../subsys/mmw/mmw_point_cloud/mmw_process/common_lib $(IncludeSwitch)../../../../subsys/mmw/mmw_point_cloud/mmw_process/mmw_psic_lib $(IncludeSwitch)../../../../subsys/mmw/mmw_point_cloud/mmw_process/mmw_psic_lib/cluster $(IncludeSwitch)../../../../subsys/mmw/mmw_point_cloud/test $(IncludeSwitch)../../../../subsys/mmw/mmw_psic_lib $(IncludeSwitch)../../../../subsys/mmw/mmw_psic_lib/cluster $(IncludeSwitch)../../../../subsys/ota/inc $(IncludeSwitch)../../../../subsys/pm/include $(IncludeSwitch)../../../../subsys/pm/port $(IncludeSwitch)../../../../subsys/pm/sys $(IncludeSwitch)../../../../subsys/shell/letter_shell/inc $(IncludeSwitch)../../../../subsys/test $(IncludeSwitch)../../../../subsys/testsuite/inc $(IncludeSwitch)../app/inc  
Libs                   := -Wl,--whole-archive  $(LibrarySwitch)m $(LibrarySwitch)dsp $(LibrarySwitch)point_cloud  -Wl,--no-whole-archive 
ArLibs                 := "m" "dsp" "point_cloud" 
PackagesLibPath        :=
LibPath                :=$(LibraryPathSwitch)../../../../libs/dsp/ $(LibraryPathSwitch)../../../../libs/mmw/mmw_alg/point_cloud/rs6130/  $(PackagesLibPath) 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       :=riscv64-unknown-elf-ar rcu
CXX      :=riscv64-unknown-elf-g++
CC       :=riscv64-unknown-elf-gcc
AS       :=riscv64-unknown-elf-gcc
OBJDUMP  :=riscv64-unknown-elf-objdump
OBJCOPY  :=riscv64-unknown-elf-objcopy
CXXFLAGS :=-mcpu=e906f   $(PreprocessorSwitch)CONFIG_BOARD_MRS6130_P1812=1  -Os   -Wall -MP -MMD -Os -Wno-main -mcmodel=medlow -Wno-unused-function -Wpointer-arith -Wno-undef -Wall -ffunction-sections -fdata-sections -fno-inline-functions -fno-builtin -fno-strict-aliasing -pipe 
CFLAGS   :=-mcpu=e906f   $(PreprocessorSwitch)CONFIG_BOARD_MRS6130_P1812=1  -Os   -Wall -MP -MMD -Os -Wno-main -mcmodel=medlow -Wno-unused-function -Wpointer-arith -Wno-undef -Wall -ffunction-sections -fdata-sections -fno-inline-functions -fno-builtin -fno-strict-aliasing -pipe 
ASFLAGS  :=-mcpu=e906f   $(PreprocessorSwitch)CONFIG_BOARD_MRS6130_P1812=1  -MP -MMD -Os -Wno-main -mcmodel=medlow -pipe 
PreprocessFlags  :=-mcpu=e906f   $(PreprocessorSwitch)CONFIG_BOARD_MRS6130_P1812=1  -Os   -Wall -MP -MMD -Os -Wno-main -mcmodel=medlow -Wno-unused-function -Wpointer-arith -Wno-undef -Wall -ffunction-sections -fdata-sections -fno-inline-functions -fno-builtin -fno-strict-aliasing -pipe 


Objects0=$(IntermediateDirectory)/boards_board$(ObjectSuffix) $(IntermediateDirectory)/utils_image_packet$(ObjectSuffix) $(IntermediateDirectory)/utils_version$(ObjectSuffix) $(IntermediateDirectory)/cmd_cmd_clk$(ObjectSuffix) $(IntermediateDirectory)/cmd_cmd_dma$(ObjectSuffix) $(IntermediateDirectory)/cmd_cmd_efuse$(ObjectSuffix) $(IntermediateDirectory)/cmd_cmd_flash$(ObjectSuffix) $(IntermediateDirectory)/cmd_cmd_gpio$(ObjectSuffix) $(IntermediateDirectory)/cmd_cmd_heap$(ObjectSuffix) $(IntermediateDirectory)/cmd_cmd_i2c$(ObjectSuffix) \
	$(IntermediateDirectory)/cmd_cmd_kernel$(ObjectSuffix) $(IntermediateDirectory)/cmd_cmd_kvf$(ObjectSuffix) $(IntermediateDirectory)/cmd_cmd_mem$(ObjectSuffix) $(IntermediateDirectory)/cmd_cmd_pm$(ObjectSuffix) $(IntermediateDirectory)/cmd_cmd_spi$(ObjectSuffix) $(IntermediateDirectory)/cmd_cmd_uart$(ObjectSuffix) $(IntermediateDirectory)/cmd_cmd_version$(ObjectSuffix) $(IntermediateDirectory)/cmd_cmd_wdg$(ObjectSuffix) $(IntermediateDirectory)/cmd_cmd_wkio$(ObjectSuffix) $(IntermediateDirectory)/libc_lib_msleep$(ObjectSuffix) \
	$(IntermediateDirectory)/libc_lib_sleep$(ObjectSuffix) $(IntermediateDirectory)/libc_lib_usleep$(ObjectSuffix) $(IntermediateDirectory)/libc_newlib_stub$(ObjectSuffix) $(IntermediateDirectory)/libc_printf$(ObjectSuffix) $(IntermediateDirectory)/libc_random$(ObjectSuffix) $(IntermediateDirectory)/libc_settimeofday$(ObjectSuffix) $(IntermediateDirectory)/pm_device$(ObjectSuffix) $(IntermediateDirectory)/pm_device_runtime$(ObjectSuffix) $(IntermediateDirectory)/pm_pm$(ObjectSuffix) $(IntermediateDirectory)/pm_pm_shell$(ObjectSuffix) \
	$(IntermediateDirectory)/pm_policy$(ObjectSuffix) $(IntermediateDirectory)/pm_test$(ObjectSuffix) $(IntermediateDirectory)/test_test_kvf$(ObjectSuffix) $(IntermediateDirectory)/portable_heap_4$(ObjectSuffix) $(IntermediateDirectory)/portable_port$(ObjectSuffix) $(IntermediateDirectory)/portable_port_riscv$(ObjectSuffix) $(IntermediateDirectory)/src_croutine$(ObjectSuffix) $(IntermediateDirectory)/src_event_groups$(ObjectSuffix) $(IntermediateDirectory)/src_list$(ObjectSuffix) $(IntermediateDirectory)/src_queue$(ObjectSuffix) \
	$(IntermediateDirectory)/src_stream_buffer$(ObjectSuffix) $(IntermediateDirectory)/src_tasks$(ObjectSuffix) $(IntermediateDirectory)/src_timers$(ObjectSuffix) $(IntermediateDirectory)/freeRTOS_osi_memory$(ObjectSuffix) $(IntermediateDirectory)/freeRTOS_osi_mutex$(ObjectSuffix) $(IntermediateDirectory)/freeRTOS_osi_port$(ObjectSuffix) $(IntermediateDirectory)/freeRTOS_osi_queue$(ObjectSuffix) $(IntermediateDirectory)/freeRTOS_osi_semaphore$(ObjectSuffix) 

Objects1=$(IntermediateDirectory)/freeRTOS_osi_thread$(ObjectSuffix) $(IntermediateDirectory)/freeRTOS_osi_timer$(ObjectSuffix) \
	$(IntermediateDirectory)/mrs6130_p1806_mrs6130_p1806$(ObjectSuffix) $(IntermediateDirectory)/mrs6130_p1812_mrs6130_p1812$(ObjectSuffix) $(IntermediateDirectory)/mrs6240_p2512_mrs6240_p2512$(ObjectSuffix) $(IntermediateDirectory)/src_hal_board$(ObjectSuffix) $(IntermediateDirectory)/src_hal_clock$(ObjectSuffix) $(IntermediateDirectory)/src_hal_clock_calib$(ObjectSuffix) $(IntermediateDirectory)/src_hal_dev$(ObjectSuffix) $(IntermediateDirectory)/src_hal_dma$(ObjectSuffix) $(IntermediateDirectory)/src_hal_efuse$(ObjectSuffix) $(IntermediateDirectory)/src_hal_flash$(ObjectSuffix) \
	$(IntermediateDirectory)/src_hal_flash_chip$(ObjectSuffix) $(IntermediateDirectory)/src_hal_flash_jesd216$(ObjectSuffix) $(IntermediateDirectory)/src_hal_gpio$(ObjectSuffix) $(IntermediateDirectory)/src_hal_i2c$(ObjectSuffix) $(IntermediateDirectory)/src_hal_power$(ObjectSuffix) $(IntermediateDirectory)/src_hal_spi$(ObjectSuffix) $(IntermediateDirectory)/src_hal_uart$(ObjectSuffix) $(IntermediateDirectory)/src_hal_wdg$(ObjectSuffix) $(IntermediateDirectory)/src_hal_wkio$(ObjectSuffix) $(IntermediateDirectory)/src_main$(ObjectSuffix) \
	$(IntermediateDirectory)/src_sleep_detect$(ObjectSuffix) $(IntermediateDirectory)/src_factory_test$(ObjectSuffix) $(IntermediateDirectory)/src_mp_ctrl$(ObjectSuffix) $(IntermediateDirectory)/src_hif$(ObjectSuffix) $(IntermediateDirectory)/src_hif_checksum$(ObjectSuffix) $(IntermediateDirectory)/src_hif_com$(ObjectSuffix) $(IntermediateDirectory)/src_hif_com_dma$(ObjectSuffix) $(IntermediateDirectory)/src_hif_com_iic$(ObjectSuffix) $(IntermediateDirectory)/src_hif_com_spi$(ObjectSuffix) $(IntermediateDirectory)/src_hif_com_uart$(ObjectSuffix) \
	$(IntermediateDirectory)/src_hif_io$(ObjectSuffix) $(IntermediateDirectory)/src_hif_mem$(ObjectSuffix) $(IntermediateDirectory)/src_hif_msg$(ObjectSuffix) $(IntermediateDirectory)/src_hif_pm$(ObjectSuffix) $(IntermediateDirectory)/string_strext$(ObjectSuffix) $(IntermediateDirectory)/sys_fcntl$(ObjectSuffix) $(IntermediateDirectory)/sys_list$(ObjectSuffix) $(IntermediateDirectory)/src_log_io$(ObjectSuffix) $(IntermediateDirectory)/src_printf_early_port$(ObjectSuffix) $(IntermediateDirectory)/src_printf_port$(ObjectSuffix) \
	$(IntermediateDirectory)/mmw_algorithm_cfar_casogo$(ObjectSuffix) $(IntermediateDirectory)/mmw_algorithm_mmw_alg_ant_calibration$(ObjectSuffix) $(IntermediateDirectory)/mmw_algorithm_mmw_alg_debug$(ObjectSuffix) $(IntermediateDirectory)/mmw_algorithm_mmw_alg_doa$(ObjectSuffix) 

Objects2=$(IntermediateDirectory)/mmw_algorithm_mmw_alg_micro_cube$(ObjectSuffix) $(IntermediateDirectory)/mmw_algorithm_mmw_alg_pointcloud$(ObjectSuffix) $(IntermediateDirectory)/mmw_application_mmw_app_micro_pointcloud$(ObjectSuffix) $(IntermediateDirectory)/mmw_application_mmw_app_pointcloud$(ObjectSuffix) $(IntermediateDirectory)/mmw_application_mmw_app_pointcloud_config$(ObjectSuffix) $(IntermediateDirectory)/mmw_mdsp_fast_func$(ObjectSuffix) \
	$(IntermediateDirectory)/mmw_mdsp_fft_func$(ObjectSuffix) $(IntermediateDirectory)/mmw_mdsp_HwAux$(ObjectSuffix) $(IntermediateDirectory)/mmw_mdsp_Luts$(ObjectSuffix) $(IntermediateDirectory)/port_port$(ObjectSuffix) $(IntermediateDirectory)/src_mtest$(ObjectSuffix) $(IntermediateDirectory)/src_sleep$(ObjectSuffix) $(IntermediateDirectory)/src_startup$(ObjectSuffix) $(IntermediateDirectory)/src_startup_c$(ObjectSuffix) $(IntermediateDirectory)/src_trap$(ObjectSuffix) $(IntermediateDirectory)/src_trap_c$(ObjectSuffix) \
	$(IntermediateDirectory)/src_vectors$(ObjectSuffix) 

Objects3=$(IntermediateDirectory)/src_irq$(ObjectSuffix) $(IntermediateDirectory)/src_pmu_tmr$(ObjectSuffix) $(IntermediateDirectory)/src_soc$(ObjectSuffix) $(IntermediateDirectory)/src_rs613x$(ObjectSuffix) $(IntermediateDirectory)/src_rs624x$(ObjectSuffix) $(IntermediateDirectory)/src_kvf$(ObjectSuffix) $(IntermediateDirectory)/src_nvs$(ObjectSuffix) $(IntermediateDirectory)/src_kvr$(ObjectSuffix) $(IntermediateDirectory)/test_mmw_point_cloud_ut$(ObjectSuffix) \
	$(IntermediateDirectory)/test_mmw_point_cloud_vv$(ObjectSuffix) $(IntermediateDirectory)/src_mmw_hif$(ObjectSuffix) $(IntermediateDirectory)/src_mmw_report$(ObjectSuffix) $(IntermediateDirectory)/src_mmw_shell$(ObjectSuffix) $(IntermediateDirectory)/src_mmw_ctrl$(ObjectSuffix) $(IntermediateDirectory)/src_mmw_dev$(ObjectSuffix) $(IntermediateDirectory)/src_mmw_dev_shell$(ObjectSuffix) $(IntermediateDirectory)/src_mmw_low_power$(ObjectSuffix) $(IntermediateDirectory)/src_mmw_pal$(ObjectSuffix) $(IntermediateDirectory)/src_mmw_temp_mgmt$(ObjectSuffix) \
	$(IntermediateDirectory)/src_shell$(ObjectSuffix) $(IntermediateDirectory)/src_shell_cmd_group$(ObjectSuffix) $(IntermediateDirectory)/src_shell_cmd_list$(ObjectSuffix) $(IntermediateDirectory)/src_shell_companion$(ObjectSuffix) $(IntermediateDirectory)/src_shell_ext$(ObjectSuffix) $(IntermediateDirectory)/src_shell_port$(ObjectSuffix) 



Objects=$(Objects0) $(Objects1) $(Objects2) $(Objects3) 

##
## Main Build Targets 
##
.PHONY: all
HeartObjects=$(IntermediateDirectory)/src_v6_heart_device$(ObjectSuffix)
Objects1 += $(HeartObjects)
all: $(IntermediateDirectory)/$(OutputFile)

$(IntermediateDirectory)/$(OutputFile):  $(Objects) Always_Link 
	$(LinkerName) $(OutputSwitch) $(IntermediateDirectory)/$(OutputFile)$(ExeSuffix) $(LinkerNameoption) -Wl,-Map=$(ProjectPath)/Lst/$(OutputFile).map  @$(ObjectsFileList) $(HeartObjects) $(LinkOptions) $(LibPath) $(Libs) $(LinkOtherFlagsOption)
	-@mv $(ProjectPath)/Lst/$(OutputFile).map $(ProjectPath)/Lst/$(OutputFile).temp && $(READELF) $(ElfInfoSwitch) $(ProjectPath)/Obj/$(OutputFile)$(ExeSuffix) > $(ProjectPath)/Lst/$(OutputFile).map && echo ====================================================================== >> $(ProjectPath)/Lst/$(OutputFile).map && cat $(ProjectPath)/Lst/$(OutputFile).temp >> $(ProjectPath)/Lst/$(OutputFile).map
	$(OBJDUMP) $(ObjdumpSwitch) $(ProjectPath)/$(IntermediateDirectory)/$(OutputFile)$(ExeSuffix)  > $(ProjectPath)/Lst/$(OutputFile)$(DisassemSuffix) 
	@echo size of target:
	@$(SIZE) $(ProjectPath)$(IntermediateDirectory)/$(OutputFile)$(ExeSuffix) 
	@echo -n checksum value of target:  
	@$(CHECKSUM) $(ProjectPath)/$(IntermediateDirectory)/$(OutputFile)$(ExeSuffix) 

Always_Link:


##
## Objects
##
$(IntermediateDirectory)/boards_board$(ObjectSuffix): ../../../../platform/boards/board.c  
	$(CC) $(SourceSwitch) ../../../../platform/boards/board.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/boards_board$(ObjectSuffix) -MF$(IntermediateDirectory)/boards_board$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/boards_board$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/boards_board$(PreprocessSuffix): ../../../../platform/boards/board.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/boards_board$(PreprocessSuffix) ../../../../platform/boards/board.c

$(IntermediateDirectory)/utils_image_packet$(ObjectSuffix): ../../../../platform/utils/image_packet.c  
	$(CC) $(SourceSwitch) ../../../../platform/utils/image_packet.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/utils_image_packet$(ObjectSuffix) -MF$(IntermediateDirectory)/utils_image_packet$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/utils_image_packet$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/utils_image_packet$(PreprocessSuffix): ../../../../platform/utils/image_packet.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/utils_image_packet$(PreprocessSuffix) ../../../../platform/utils/image_packet.c

$(IntermediateDirectory)/utils_version$(ObjectSuffix): ../../../../platform/utils/version.c  
	$(CC) $(SourceSwitch) ../../../../platform/utils/version.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/utils_version$(ObjectSuffix) -MF$(IntermediateDirectory)/utils_version$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/utils_version$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/utils_version$(PreprocessSuffix): ../../../../platform/utils/version.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/utils_version$(PreprocessSuffix) ../../../../platform/utils/version.c

$(IntermediateDirectory)/cmd_cmd_clk$(ObjectSuffix): ../../../../subsys/cmd/cmd_clk.c  
	$(CC) $(SourceSwitch) ../../../../subsys/cmd/cmd_clk.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/cmd_cmd_clk$(ObjectSuffix) -MF$(IntermediateDirectory)/cmd_cmd_clk$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/cmd_cmd_clk$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/cmd_cmd_clk$(PreprocessSuffix): ../../../../subsys/cmd/cmd_clk.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/cmd_cmd_clk$(PreprocessSuffix) ../../../../subsys/cmd/cmd_clk.c

$(IntermediateDirectory)/cmd_cmd_dma$(ObjectSuffix): ../../../../subsys/cmd/cmd_dma.c  
	$(CC) $(SourceSwitch) ../../../../subsys/cmd/cmd_dma.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/cmd_cmd_dma$(ObjectSuffix) -MF$(IntermediateDirectory)/cmd_cmd_dma$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/cmd_cmd_dma$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/cmd_cmd_dma$(PreprocessSuffix): ../../../../subsys/cmd/cmd_dma.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/cmd_cmd_dma$(PreprocessSuffix) ../../../../subsys/cmd/cmd_dma.c

$(IntermediateDirectory)/cmd_cmd_efuse$(ObjectSuffix): ../../../../subsys/cmd/cmd_efuse.c  
	$(CC) $(SourceSwitch) ../../../../subsys/cmd/cmd_efuse.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/cmd_cmd_efuse$(ObjectSuffix) -MF$(IntermediateDirectory)/cmd_cmd_efuse$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/cmd_cmd_efuse$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/cmd_cmd_efuse$(PreprocessSuffix): ../../../../subsys/cmd/cmd_efuse.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/cmd_cmd_efuse$(PreprocessSuffix) ../../../../subsys/cmd/cmd_efuse.c

$(IntermediateDirectory)/cmd_cmd_flash$(ObjectSuffix): ../../../../subsys/cmd/cmd_flash.c  
	$(CC) $(SourceSwitch) ../../../../subsys/cmd/cmd_flash.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/cmd_cmd_flash$(ObjectSuffix) -MF$(IntermediateDirectory)/cmd_cmd_flash$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/cmd_cmd_flash$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/cmd_cmd_flash$(PreprocessSuffix): ../../../../subsys/cmd/cmd_flash.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/cmd_cmd_flash$(PreprocessSuffix) ../../../../subsys/cmd/cmd_flash.c

$(IntermediateDirectory)/cmd_cmd_gpio$(ObjectSuffix): ../../../../subsys/cmd/cmd_gpio.c  
	$(CC) $(SourceSwitch) ../../../../subsys/cmd/cmd_gpio.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/cmd_cmd_gpio$(ObjectSuffix) -MF$(IntermediateDirectory)/cmd_cmd_gpio$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/cmd_cmd_gpio$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/cmd_cmd_gpio$(PreprocessSuffix): ../../../../subsys/cmd/cmd_gpio.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/cmd_cmd_gpio$(PreprocessSuffix) ../../../../subsys/cmd/cmd_gpio.c

$(IntermediateDirectory)/cmd_cmd_heap$(ObjectSuffix): ../../../../subsys/cmd/cmd_heap.c  
	$(CC) $(SourceSwitch) ../../../../subsys/cmd/cmd_heap.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/cmd_cmd_heap$(ObjectSuffix) -MF$(IntermediateDirectory)/cmd_cmd_heap$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/cmd_cmd_heap$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/cmd_cmd_heap$(PreprocessSuffix): ../../../../subsys/cmd/cmd_heap.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/cmd_cmd_heap$(PreprocessSuffix) ../../../../subsys/cmd/cmd_heap.c

$(IntermediateDirectory)/cmd_cmd_i2c$(ObjectSuffix): ../../../../subsys/cmd/cmd_i2c.c  
	$(CC) $(SourceSwitch) ../../../../subsys/cmd/cmd_i2c.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/cmd_cmd_i2c$(ObjectSuffix) -MF$(IntermediateDirectory)/cmd_cmd_i2c$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/cmd_cmd_i2c$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/cmd_cmd_i2c$(PreprocessSuffix): ../../../../subsys/cmd/cmd_i2c.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/cmd_cmd_i2c$(PreprocessSuffix) ../../../../subsys/cmd/cmd_i2c.c

$(IntermediateDirectory)/cmd_cmd_kernel$(ObjectSuffix): ../../../../subsys/cmd/cmd_kernel.c  
	$(CC) $(SourceSwitch) ../../../../subsys/cmd/cmd_kernel.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/cmd_cmd_kernel$(ObjectSuffix) -MF$(IntermediateDirectory)/cmd_cmd_kernel$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/cmd_cmd_kernel$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/cmd_cmd_kernel$(PreprocessSuffix): ../../../../subsys/cmd/cmd_kernel.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/cmd_cmd_kernel$(PreprocessSuffix) ../../../../subsys/cmd/cmd_kernel.c

$(IntermediateDirectory)/cmd_cmd_kvf$(ObjectSuffix): ../../../../subsys/cmd/cmd_kvf.c  
	$(CC) $(SourceSwitch) ../../../../subsys/cmd/cmd_kvf.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/cmd_cmd_kvf$(ObjectSuffix) -MF$(IntermediateDirectory)/cmd_cmd_kvf$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/cmd_cmd_kvf$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/cmd_cmd_kvf$(PreprocessSuffix): ../../../../subsys/cmd/cmd_kvf.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/cmd_cmd_kvf$(PreprocessSuffix) ../../../../subsys/cmd/cmd_kvf.c

$(IntermediateDirectory)/cmd_cmd_mem$(ObjectSuffix): ../../../../subsys/cmd/cmd_mem.c  
	$(CC) $(SourceSwitch) ../../../../subsys/cmd/cmd_mem.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/cmd_cmd_mem$(ObjectSuffix) -MF$(IntermediateDirectory)/cmd_cmd_mem$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/cmd_cmd_mem$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/cmd_cmd_mem$(PreprocessSuffix): ../../../../subsys/cmd/cmd_mem.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/cmd_cmd_mem$(PreprocessSuffix) ../../../../subsys/cmd/cmd_mem.c

$(IntermediateDirectory)/cmd_cmd_pm$(ObjectSuffix): ../../../../subsys/cmd/cmd_pm.c  
	$(CC) $(SourceSwitch) ../../../../subsys/cmd/cmd_pm.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/cmd_cmd_pm$(ObjectSuffix) -MF$(IntermediateDirectory)/cmd_cmd_pm$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/cmd_cmd_pm$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/cmd_cmd_pm$(PreprocessSuffix): ../../../../subsys/cmd/cmd_pm.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/cmd_cmd_pm$(PreprocessSuffix) ../../../../subsys/cmd/cmd_pm.c

$(IntermediateDirectory)/cmd_cmd_spi$(ObjectSuffix): ../../../../subsys/cmd/cmd_spi.c  
	$(CC) $(SourceSwitch) ../../../../subsys/cmd/cmd_spi.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/cmd_cmd_spi$(ObjectSuffix) -MF$(IntermediateDirectory)/cmd_cmd_spi$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/cmd_cmd_spi$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/cmd_cmd_spi$(PreprocessSuffix): ../../../../subsys/cmd/cmd_spi.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/cmd_cmd_spi$(PreprocessSuffix) ../../../../subsys/cmd/cmd_spi.c

$(IntermediateDirectory)/cmd_cmd_uart$(ObjectSuffix): ../../../../subsys/cmd/cmd_uart.c  
	$(CC) $(SourceSwitch) ../../../../subsys/cmd/cmd_uart.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/cmd_cmd_uart$(ObjectSuffix) -MF$(IntermediateDirectory)/cmd_cmd_uart$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/cmd_cmd_uart$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/cmd_cmd_uart$(PreprocessSuffix): ../../../../subsys/cmd/cmd_uart.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/cmd_cmd_uart$(PreprocessSuffix) ../../../../subsys/cmd/cmd_uart.c

$(IntermediateDirectory)/cmd_cmd_version$(ObjectSuffix): ../../../../subsys/cmd/cmd_version.c  
	$(CC) $(SourceSwitch) ../../../../subsys/cmd/cmd_version.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/cmd_cmd_version$(ObjectSuffix) -MF$(IntermediateDirectory)/cmd_cmd_version$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/cmd_cmd_version$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/cmd_cmd_version$(PreprocessSuffix): ../../../../subsys/cmd/cmd_version.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/cmd_cmd_version$(PreprocessSuffix) ../../../../subsys/cmd/cmd_version.c

$(IntermediateDirectory)/cmd_cmd_wdg$(ObjectSuffix): ../../../../subsys/cmd/cmd_wdg.c  
	$(CC) $(SourceSwitch) ../../../../subsys/cmd/cmd_wdg.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/cmd_cmd_wdg$(ObjectSuffix) -MF$(IntermediateDirectory)/cmd_cmd_wdg$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/cmd_cmd_wdg$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/cmd_cmd_wdg$(PreprocessSuffix): ../../../../subsys/cmd/cmd_wdg.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/cmd_cmd_wdg$(PreprocessSuffix) ../../../../subsys/cmd/cmd_wdg.c

$(IntermediateDirectory)/cmd_cmd_wkio$(ObjectSuffix): ../../../../subsys/cmd/cmd_wkio.c  
	$(CC) $(SourceSwitch) ../../../../subsys/cmd/cmd_wkio.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/cmd_cmd_wkio$(ObjectSuffix) -MF$(IntermediateDirectory)/cmd_cmd_wkio$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/cmd_cmd_wkio$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/cmd_cmd_wkio$(PreprocessSuffix): ../../../../subsys/cmd/cmd_wkio.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/cmd_cmd_wkio$(PreprocessSuffix) ../../../../subsys/cmd/cmd_wkio.c

$(IntermediateDirectory)/libc_lib_msleep$(ObjectSuffix): ../../../../subsys/libc/lib_msleep.c  
	$(CC) $(SourceSwitch) ../../../../subsys/libc/lib_msleep.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/libc_lib_msleep$(ObjectSuffix) -MF$(IntermediateDirectory)/libc_lib_msleep$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/libc_lib_msleep$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/libc_lib_msleep$(PreprocessSuffix): ../../../../subsys/libc/lib_msleep.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/libc_lib_msleep$(PreprocessSuffix) ../../../../subsys/libc/lib_msleep.c

$(IntermediateDirectory)/libc_lib_sleep$(ObjectSuffix): ../../../../subsys/libc/lib_sleep.c  
	$(CC) $(SourceSwitch) ../../../../subsys/libc/lib_sleep.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/libc_lib_sleep$(ObjectSuffix) -MF$(IntermediateDirectory)/libc_lib_sleep$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/libc_lib_sleep$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/libc_lib_sleep$(PreprocessSuffix): ../../../../subsys/libc/lib_sleep.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/libc_lib_sleep$(PreprocessSuffix) ../../../../subsys/libc/lib_sleep.c

$(IntermediateDirectory)/libc_lib_usleep$(ObjectSuffix): ../../../../subsys/libc/lib_usleep.c  
	$(CC) $(SourceSwitch) ../../../../subsys/libc/lib_usleep.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/libc_lib_usleep$(ObjectSuffix) -MF$(IntermediateDirectory)/libc_lib_usleep$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/libc_lib_usleep$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/libc_lib_usleep$(PreprocessSuffix): ../../../../subsys/libc/lib_usleep.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/libc_lib_usleep$(PreprocessSuffix) ../../../../subsys/libc/lib_usleep.c

$(IntermediateDirectory)/libc_newlib_stub$(ObjectSuffix): ../../../../subsys/libc/newlib_stub.c  
	$(CC) $(SourceSwitch) ../../../../subsys/libc/newlib_stub.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/libc_newlib_stub$(ObjectSuffix) -MF$(IntermediateDirectory)/libc_newlib_stub$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/libc_newlib_stub$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/libc_newlib_stub$(PreprocessSuffix): ../../../../subsys/libc/newlib_stub.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/libc_newlib_stub$(PreprocessSuffix) ../../../../subsys/libc/newlib_stub.c

$(IntermediateDirectory)/libc_printf$(ObjectSuffix): ../../../../subsys/libc/printf.c  
	$(CC) $(SourceSwitch) ../../../../subsys/libc/printf.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/libc_printf$(ObjectSuffix) -MF$(IntermediateDirectory)/libc_printf$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/libc_printf$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/libc_printf$(PreprocessSuffix): ../../../../subsys/libc/printf.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/libc_printf$(PreprocessSuffix) ../../../../subsys/libc/printf.c

$(IntermediateDirectory)/libc_random$(ObjectSuffix): ../../../../subsys/libc/random.c  
	$(CC) $(SourceSwitch) ../../../../subsys/libc/random.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/libc_random$(ObjectSuffix) -MF$(IntermediateDirectory)/libc_random$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/libc_random$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/libc_random$(PreprocessSuffix): ../../../../subsys/libc/random.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/libc_random$(PreprocessSuffix) ../../../../subsys/libc/random.c

$(IntermediateDirectory)/libc_settimeofday$(ObjectSuffix): ../../../../subsys/libc/settimeofday.c  
	$(CC) $(SourceSwitch) ../../../../subsys/libc/settimeofday.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/libc_settimeofday$(ObjectSuffix) -MF$(IntermediateDirectory)/libc_settimeofday$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/libc_settimeofday$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/libc_settimeofday$(PreprocessSuffix): ../../../../subsys/libc/settimeofday.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/libc_settimeofday$(PreprocessSuffix) ../../../../subsys/libc/settimeofday.c

$(IntermediateDirectory)/pm_device$(ObjectSuffix): ../../../../subsys/pm/device.c  
	$(CC) $(SourceSwitch) ../../../../subsys/pm/device.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/pm_device$(ObjectSuffix) -MF$(IntermediateDirectory)/pm_device$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/pm_device$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/pm_device$(PreprocessSuffix): ../../../../subsys/pm/device.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/pm_device$(PreprocessSuffix) ../../../../subsys/pm/device.c

$(IntermediateDirectory)/pm_device_runtime$(ObjectSuffix): ../../../../subsys/pm/device_runtime.c  
	$(CC) $(SourceSwitch) ../../../../subsys/pm/device_runtime.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/pm_device_runtime$(ObjectSuffix) -MF$(IntermediateDirectory)/pm_device_runtime$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/pm_device_runtime$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/pm_device_runtime$(PreprocessSuffix): ../../../../subsys/pm/device_runtime.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/pm_device_runtime$(PreprocessSuffix) ../../../../subsys/pm/device_runtime.c

$(IntermediateDirectory)/pm_pm$(ObjectSuffix): ../../../../subsys/pm/pm.c  
	$(CC) $(SourceSwitch) ../../../../subsys/pm/pm.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/pm_pm$(ObjectSuffix) -MF$(IntermediateDirectory)/pm_pm$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/pm_pm$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/pm_pm$(PreprocessSuffix): ../../../../subsys/pm/pm.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/pm_pm$(PreprocessSuffix) ../../../../subsys/pm/pm.c

$(IntermediateDirectory)/pm_pm_shell$(ObjectSuffix): ../../../../subsys/pm/pm_shell.c  
	$(CC) $(SourceSwitch) ../../../../subsys/pm/pm_shell.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/pm_pm_shell$(ObjectSuffix) -MF$(IntermediateDirectory)/pm_pm_shell$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/pm_pm_shell$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/pm_pm_shell$(PreprocessSuffix): ../../../../subsys/pm/pm_shell.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/pm_pm_shell$(PreprocessSuffix) ../../../../subsys/pm/pm_shell.c

$(IntermediateDirectory)/pm_policy$(ObjectSuffix): ../../../../subsys/pm/policy.c  
	$(CC) $(SourceSwitch) ../../../../subsys/pm/policy.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/pm_policy$(ObjectSuffix) -MF$(IntermediateDirectory)/pm_policy$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/pm_policy$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/pm_policy$(PreprocessSuffix): ../../../../subsys/pm/policy.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/pm_policy$(PreprocessSuffix) ../../../../subsys/pm/policy.c

$(IntermediateDirectory)/pm_test$(ObjectSuffix): ../../../../subsys/pm/test.c  
	$(CC) $(SourceSwitch) ../../../../subsys/pm/test.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/pm_test$(ObjectSuffix) -MF$(IntermediateDirectory)/pm_test$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/pm_test$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/pm_test$(PreprocessSuffix): ../../../../subsys/pm/test.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/pm_test$(PreprocessSuffix) ../../../../subsys/pm/test.c

$(IntermediateDirectory)/test_test_kvf$(ObjectSuffix): ../../../../subsys/test/test_kvf.c  
	$(CC) $(SourceSwitch) ../../../../subsys/test/test_kvf.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/test_test_kvf$(ObjectSuffix) -MF$(IntermediateDirectory)/test_test_kvf$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/test_test_kvf$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/test_test_kvf$(PreprocessSuffix): ../../../../subsys/test/test_kvf.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/test_test_kvf$(PreprocessSuffix) ../../../../subsys/test/test_kvf.c

$(IntermediateDirectory)/portable_heap_4$(ObjectSuffix): ../../../../kernel/freeRTOS/portable/heap_4.c  
	$(CC) $(SourceSwitch) ../../../../kernel/freeRTOS/portable/heap_4.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/portable_heap_4$(ObjectSuffix) -MF$(IntermediateDirectory)/portable_heap_4$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/portable_heap_4$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/portable_heap_4$(PreprocessSuffix): ../../../../kernel/freeRTOS/portable/heap_4.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/portable_heap_4$(PreprocessSuffix) ../../../../kernel/freeRTOS/portable/heap_4.c

$(IntermediateDirectory)/portable_port$(ObjectSuffix): ../../../../kernel/freeRTOS/portable/port.c  
	$(CC) $(SourceSwitch) ../../../../kernel/freeRTOS/portable/port.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/portable_port$(ObjectSuffix) -MF$(IntermediateDirectory)/portable_port$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/portable_port$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/portable_port$(PreprocessSuffix): ../../../../kernel/freeRTOS/portable/port.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/portable_port$(PreprocessSuffix) ../../../../kernel/freeRTOS/portable/port.c

$(IntermediateDirectory)/portable_port_riscv$(ObjectSuffix): ../../../../kernel/freeRTOS/portable/port_riscv.S  
	$(AS) $(SourceSwitch) ../../../../kernel/freeRTOS/portable/port_riscv.S $(ASFLAGS) -MMD -MP -MT$(IntermediateDirectory)/portable_port_riscv$(ObjectSuffix) -MF$(IntermediateDirectory)/portable_port_riscv$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/portable_port_riscv$(ObjectSuffix) $(IncludeAPath) $(IncludePackagePath)
Lst/portable_port_riscv$(PreprocessSuffix): ../../../../kernel/freeRTOS/portable/port_riscv.S
	$(CC) $(CFLAGS)$(IncludeAPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/portable_port_riscv$(PreprocessSuffix) ../../../../kernel/freeRTOS/portable/port_riscv.S

$(IntermediateDirectory)/src_croutine$(ObjectSuffix): ../../../../kernel/freeRTOS/src/croutine.c  
	$(CC) $(SourceSwitch) ../../../../kernel/freeRTOS/src/croutine.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_croutine$(ObjectSuffix) -MF$(IntermediateDirectory)/src_croutine$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_croutine$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_croutine$(PreprocessSuffix): ../../../../kernel/freeRTOS/src/croutine.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_croutine$(PreprocessSuffix) ../../../../kernel/freeRTOS/src/croutine.c

$(IntermediateDirectory)/src_event_groups$(ObjectSuffix): ../../../../kernel/freeRTOS/src/event_groups.c  
	$(CC) $(SourceSwitch) ../../../../kernel/freeRTOS/src/event_groups.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_event_groups$(ObjectSuffix) -MF$(IntermediateDirectory)/src_event_groups$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_event_groups$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_event_groups$(PreprocessSuffix): ../../../../kernel/freeRTOS/src/event_groups.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_event_groups$(PreprocessSuffix) ../../../../kernel/freeRTOS/src/event_groups.c

$(IntermediateDirectory)/src_list$(ObjectSuffix): ../../../../kernel/freeRTOS/src/list.c  
	$(CC) $(SourceSwitch) ../../../../kernel/freeRTOS/src/list.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_list$(ObjectSuffix) -MF$(IntermediateDirectory)/src_list$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_list$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_list$(PreprocessSuffix): ../../../../kernel/freeRTOS/src/list.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_list$(PreprocessSuffix) ../../../../kernel/freeRTOS/src/list.c

$(IntermediateDirectory)/src_queue$(ObjectSuffix): ../../../../kernel/freeRTOS/src/queue.c  
	$(CC) $(SourceSwitch) ../../../../kernel/freeRTOS/src/queue.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_queue$(ObjectSuffix) -MF$(IntermediateDirectory)/src_queue$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_queue$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_queue$(PreprocessSuffix): ../../../../kernel/freeRTOS/src/queue.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_queue$(PreprocessSuffix) ../../../../kernel/freeRTOS/src/queue.c

$(IntermediateDirectory)/src_stream_buffer$(ObjectSuffix): ../../../../kernel/freeRTOS/src/stream_buffer.c  
	$(CC) $(SourceSwitch) ../../../../kernel/freeRTOS/src/stream_buffer.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_stream_buffer$(ObjectSuffix) -MF$(IntermediateDirectory)/src_stream_buffer$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_stream_buffer$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_stream_buffer$(PreprocessSuffix): ../../../../kernel/freeRTOS/src/stream_buffer.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_stream_buffer$(PreprocessSuffix) ../../../../kernel/freeRTOS/src/stream_buffer.c

$(IntermediateDirectory)/src_tasks$(ObjectSuffix): ../../../../kernel/freeRTOS/src/tasks.c  
	$(CC) $(SourceSwitch) ../../../../kernel/freeRTOS/src/tasks.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_tasks$(ObjectSuffix) -MF$(IntermediateDirectory)/src_tasks$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_tasks$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_tasks$(PreprocessSuffix): ../../../../kernel/freeRTOS/src/tasks.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_tasks$(PreprocessSuffix) ../../../../kernel/freeRTOS/src/tasks.c

$(IntermediateDirectory)/src_timers$(ObjectSuffix): ../../../../kernel/freeRTOS/src/timers.c  
	$(CC) $(SourceSwitch) ../../../../kernel/freeRTOS/src/timers.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_timers$(ObjectSuffix) -MF$(IntermediateDirectory)/src_timers$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_timers$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_timers$(PreprocessSuffix): ../../../../kernel/freeRTOS/src/timers.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_timers$(PreprocessSuffix) ../../../../kernel/freeRTOS/src/timers.c

$(IntermediateDirectory)/freeRTOS_osi_memory$(ObjectSuffix): ../../../../kernel/src/freeRTOS/osi_memory.c  
	$(CC) $(SourceSwitch) ../../../../kernel/src/freeRTOS/osi_memory.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/freeRTOS_osi_memory$(ObjectSuffix) -MF$(IntermediateDirectory)/freeRTOS_osi_memory$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/freeRTOS_osi_memory$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/freeRTOS_osi_memory$(PreprocessSuffix): ../../../../kernel/src/freeRTOS/osi_memory.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/freeRTOS_osi_memory$(PreprocessSuffix) ../../../../kernel/src/freeRTOS/osi_memory.c

$(IntermediateDirectory)/freeRTOS_osi_mutex$(ObjectSuffix): ../../../../kernel/src/freeRTOS/osi_mutex.c  
	$(CC) $(SourceSwitch) ../../../../kernel/src/freeRTOS/osi_mutex.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/freeRTOS_osi_mutex$(ObjectSuffix) -MF$(IntermediateDirectory)/freeRTOS_osi_mutex$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/freeRTOS_osi_mutex$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/freeRTOS_osi_mutex$(PreprocessSuffix): ../../../../kernel/src/freeRTOS/osi_mutex.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/freeRTOS_osi_mutex$(PreprocessSuffix) ../../../../kernel/src/freeRTOS/osi_mutex.c

$(IntermediateDirectory)/freeRTOS_osi_port$(ObjectSuffix): ../../../../kernel/src/freeRTOS/osi_port.c  
	$(CC) $(SourceSwitch) ../../../../kernel/src/freeRTOS/osi_port.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/freeRTOS_osi_port$(ObjectSuffix) -MF$(IntermediateDirectory)/freeRTOS_osi_port$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/freeRTOS_osi_port$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/freeRTOS_osi_port$(PreprocessSuffix): ../../../../kernel/src/freeRTOS/osi_port.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/freeRTOS_osi_port$(PreprocessSuffix) ../../../../kernel/src/freeRTOS/osi_port.c

$(IntermediateDirectory)/freeRTOS_osi_queue$(ObjectSuffix): ../../../../kernel/src/freeRTOS/osi_queue.c  
	$(CC) $(SourceSwitch) ../../../../kernel/src/freeRTOS/osi_queue.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/freeRTOS_osi_queue$(ObjectSuffix) -MF$(IntermediateDirectory)/freeRTOS_osi_queue$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/freeRTOS_osi_queue$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/freeRTOS_osi_queue$(PreprocessSuffix): ../../../../kernel/src/freeRTOS/osi_queue.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/freeRTOS_osi_queue$(PreprocessSuffix) ../../../../kernel/src/freeRTOS/osi_queue.c

$(IntermediateDirectory)/freeRTOS_osi_semaphore$(ObjectSuffix): ../../../../kernel/src/freeRTOS/osi_semaphore.c  
	$(CC) $(SourceSwitch) ../../../../kernel/src/freeRTOS/osi_semaphore.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/freeRTOS_osi_semaphore$(ObjectSuffix) -MF$(IntermediateDirectory)/freeRTOS_osi_semaphore$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/freeRTOS_osi_semaphore$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/freeRTOS_osi_semaphore$(PreprocessSuffix): ../../../../kernel/src/freeRTOS/osi_semaphore.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/freeRTOS_osi_semaphore$(PreprocessSuffix) ../../../../kernel/src/freeRTOS/osi_semaphore.c

$(IntermediateDirectory)/freeRTOS_osi_thread$(ObjectSuffix): ../../../../kernel/src/freeRTOS/osi_thread.c  
	$(CC) $(SourceSwitch) ../../../../kernel/src/freeRTOS/osi_thread.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/freeRTOS_osi_thread$(ObjectSuffix) -MF$(IntermediateDirectory)/freeRTOS_osi_thread$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/freeRTOS_osi_thread$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/freeRTOS_osi_thread$(PreprocessSuffix): ../../../../kernel/src/freeRTOS/osi_thread.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/freeRTOS_osi_thread$(PreprocessSuffix) ../../../../kernel/src/freeRTOS/osi_thread.c

$(IntermediateDirectory)/freeRTOS_osi_timer$(ObjectSuffix): ../../../../kernel/src/freeRTOS/osi_timer.c  
	$(CC) $(SourceSwitch) ../../../../kernel/src/freeRTOS/osi_timer.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/freeRTOS_osi_timer$(ObjectSuffix) -MF$(IntermediateDirectory)/freeRTOS_osi_timer$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/freeRTOS_osi_timer$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/freeRTOS_osi_timer$(PreprocessSuffix): ../../../../kernel/src/freeRTOS/osi_timer.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/freeRTOS_osi_timer$(PreprocessSuffix) ../../../../kernel/src/freeRTOS/osi_timer.c

$(IntermediateDirectory)/mrs6130_p1806_mrs6130_p1806$(ObjectSuffix): ../../../../platform/boards/mrs6130_p1806/mrs6130_p1806.c  
	$(CC) $(SourceSwitch) ../../../../platform/boards/mrs6130_p1806/mrs6130_p1806.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/mrs6130_p1806_mrs6130_p1806$(ObjectSuffix) -MF$(IntermediateDirectory)/mrs6130_p1806_mrs6130_p1806$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/mrs6130_p1806_mrs6130_p1806$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/mrs6130_p1806_mrs6130_p1806$(PreprocessSuffix): ../../../../platform/boards/mrs6130_p1806/mrs6130_p1806.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/mrs6130_p1806_mrs6130_p1806$(PreprocessSuffix) ../../../../platform/boards/mrs6130_p1806/mrs6130_p1806.c

$(IntermediateDirectory)/mrs6130_p1812_mrs6130_p1812$(ObjectSuffix): ../../../../platform/boards/mrs6130_p1812/mrs6130_p1812.c  
	$(CC) $(SourceSwitch) ../../../../platform/boards/mrs6130_p1812/mrs6130_p1812.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/mrs6130_p1812_mrs6130_p1812$(ObjectSuffix) -MF$(IntermediateDirectory)/mrs6130_p1812_mrs6130_p1812$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/mrs6130_p1812_mrs6130_p1812$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/mrs6130_p1812_mrs6130_p1812$(PreprocessSuffix): ../../../../platform/boards/mrs6130_p1812/mrs6130_p1812.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/mrs6130_p1812_mrs6130_p1812$(PreprocessSuffix) ../../../../platform/boards/mrs6130_p1812/mrs6130_p1812.c

$(IntermediateDirectory)/mrs6240_p2512_mrs6240_p2512$(ObjectSuffix): ../../../../platform/boards/mrs6240_p2512/mrs6240_p2512.c  
	$(CC) $(SourceSwitch) ../../../../platform/boards/mrs6240_p2512/mrs6240_p2512.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/mrs6240_p2512_mrs6240_p2512$(ObjectSuffix) -MF$(IntermediateDirectory)/mrs6240_p2512_mrs6240_p2512$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/mrs6240_p2512_mrs6240_p2512$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/mrs6240_p2512_mrs6240_p2512$(PreprocessSuffix): ../../../../platform/boards/mrs6240_p2512/mrs6240_p2512.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/mrs6240_p2512_mrs6240_p2512$(PreprocessSuffix) ../../../../platform/boards/mrs6240_p2512/mrs6240_p2512.c

$(IntermediateDirectory)/src_hal_board$(ObjectSuffix): ../../../../platform/drivers/src/hal_board.c  
	$(CC) $(SourceSwitch) ../../../../platform/drivers/src/hal_board.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hal_board$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hal_board$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hal_board$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hal_board$(PreprocessSuffix): ../../../../platform/drivers/src/hal_board.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hal_board$(PreprocessSuffix) ../../../../platform/drivers/src/hal_board.c

$(IntermediateDirectory)/src_hal_clock$(ObjectSuffix): ../../../../platform/drivers/src/hal_clock.c  
	$(CC) $(SourceSwitch) ../../../../platform/drivers/src/hal_clock.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hal_clock$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hal_clock$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hal_clock$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hal_clock$(PreprocessSuffix): ../../../../platform/drivers/src/hal_clock.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hal_clock$(PreprocessSuffix) ../../../../platform/drivers/src/hal_clock.c

$(IntermediateDirectory)/src_hal_clock_calib$(ObjectSuffix): ../../../../platform/drivers/src/hal_clock_calib.c  
	$(CC) $(SourceSwitch) ../../../../platform/drivers/src/hal_clock_calib.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hal_clock_calib$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hal_clock_calib$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hal_clock_calib$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hal_clock_calib$(PreprocessSuffix): ../../../../platform/drivers/src/hal_clock_calib.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hal_clock_calib$(PreprocessSuffix) ../../../../platform/drivers/src/hal_clock_calib.c

$(IntermediateDirectory)/src_hal_dev$(ObjectSuffix): ../../../../platform/drivers/src/hal_dev.c  
	$(CC) $(SourceSwitch) ../../../../platform/drivers/src/hal_dev.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hal_dev$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hal_dev$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hal_dev$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hal_dev$(PreprocessSuffix): ../../../../platform/drivers/src/hal_dev.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hal_dev$(PreprocessSuffix) ../../../../platform/drivers/src/hal_dev.c

$(IntermediateDirectory)/src_hal_dma$(ObjectSuffix): ../../../../platform/drivers/src/hal_dma.c  
	$(CC) $(SourceSwitch) ../../../../platform/drivers/src/hal_dma.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hal_dma$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hal_dma$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hal_dma$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hal_dma$(PreprocessSuffix): ../../../../platform/drivers/src/hal_dma.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hal_dma$(PreprocessSuffix) ../../../../platform/drivers/src/hal_dma.c

$(IntermediateDirectory)/src_hal_efuse$(ObjectSuffix): ../../../../platform/drivers/src/hal_efuse.c  
	$(CC) $(SourceSwitch) ../../../../platform/drivers/src/hal_efuse.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hal_efuse$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hal_efuse$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hal_efuse$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hal_efuse$(PreprocessSuffix): ../../../../platform/drivers/src/hal_efuse.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hal_efuse$(PreprocessSuffix) ../../../../platform/drivers/src/hal_efuse.c

$(IntermediateDirectory)/src_hal_flash$(ObjectSuffix): ../../../../platform/drivers/src/hal_flash.c  
	$(CC) $(SourceSwitch) ../../../../platform/drivers/src/hal_flash.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hal_flash$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hal_flash$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hal_flash$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hal_flash$(PreprocessSuffix): ../../../../platform/drivers/src/hal_flash.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hal_flash$(PreprocessSuffix) ../../../../platform/drivers/src/hal_flash.c

$(IntermediateDirectory)/src_hal_flash_chip$(ObjectSuffix): ../../../../platform/drivers/src/hal_flash_chip.c  
	$(CC) $(SourceSwitch) ../../../../platform/drivers/src/hal_flash_chip.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hal_flash_chip$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hal_flash_chip$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hal_flash_chip$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hal_flash_chip$(PreprocessSuffix): ../../../../platform/drivers/src/hal_flash_chip.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hal_flash_chip$(PreprocessSuffix) ../../../../platform/drivers/src/hal_flash_chip.c

$(IntermediateDirectory)/src_hal_flash_jesd216$(ObjectSuffix): ../../../../platform/drivers/src/hal_flash_jesd216.c  
	$(CC) $(SourceSwitch) ../../../../platform/drivers/src/hal_flash_jesd216.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hal_flash_jesd216$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hal_flash_jesd216$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hal_flash_jesd216$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hal_flash_jesd216$(PreprocessSuffix): ../../../../platform/drivers/src/hal_flash_jesd216.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hal_flash_jesd216$(PreprocessSuffix) ../../../../platform/drivers/src/hal_flash_jesd216.c

$(IntermediateDirectory)/src_hal_gpio$(ObjectSuffix): ../../../../platform/drivers/src/hal_gpio.c  
	$(CC) $(SourceSwitch) ../../../../platform/drivers/src/hal_gpio.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hal_gpio$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hal_gpio$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hal_gpio$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hal_gpio$(PreprocessSuffix): ../../../../platform/drivers/src/hal_gpio.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hal_gpio$(PreprocessSuffix) ../../../../platform/drivers/src/hal_gpio.c

$(IntermediateDirectory)/src_hal_i2c$(ObjectSuffix): ../../../../platform/drivers/src/hal_i2c.c  
	$(CC) $(SourceSwitch) ../../../../platform/drivers/src/hal_i2c.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hal_i2c$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hal_i2c$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hal_i2c$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hal_i2c$(PreprocessSuffix): ../../../../platform/drivers/src/hal_i2c.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hal_i2c$(PreprocessSuffix) ../../../../platform/drivers/src/hal_i2c.c

$(IntermediateDirectory)/src_hal_power$(ObjectSuffix): ../../../../platform/drivers/src/hal_power.c  
	$(CC) $(SourceSwitch) ../../../../platform/drivers/src/hal_power.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hal_power$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hal_power$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hal_power$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hal_power$(PreprocessSuffix): ../../../../platform/drivers/src/hal_power.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hal_power$(PreprocessSuffix) ../../../../platform/drivers/src/hal_power.c

$(IntermediateDirectory)/src_hal_spi$(ObjectSuffix): ../../../../platform/drivers/src/hal_spi.c  
	$(CC) $(SourceSwitch) ../../../../platform/drivers/src/hal_spi.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hal_spi$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hal_spi$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hal_spi$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hal_spi$(PreprocessSuffix): ../../../../platform/drivers/src/hal_spi.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hal_spi$(PreprocessSuffix) ../../../../platform/drivers/src/hal_spi.c

$(IntermediateDirectory)/src_hal_uart$(ObjectSuffix): ../../../../platform/drivers/src/hal_uart.c  
	$(CC) $(SourceSwitch) ../../../../platform/drivers/src/hal_uart.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hal_uart$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hal_uart$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hal_uart$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hal_uart$(PreprocessSuffix): ../../../../platform/drivers/src/hal_uart.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hal_uart$(PreprocessSuffix) ../../../../platform/drivers/src/hal_uart.c

$(IntermediateDirectory)/src_hal_wdg$(ObjectSuffix): ../../../../platform/drivers/src/hal_wdg.c  
	$(CC) $(SourceSwitch) ../../../../platform/drivers/src/hal_wdg.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hal_wdg$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hal_wdg$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hal_wdg$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hal_wdg$(PreprocessSuffix): ../../../../platform/drivers/src/hal_wdg.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hal_wdg$(PreprocessSuffix) ../../../../platform/drivers/src/hal_wdg.c

$(IntermediateDirectory)/src_hal_wkio$(ObjectSuffix): ../../../../platform/drivers/src/hal_wkio.c  
	$(CC) $(SourceSwitch) ../../../../platform/drivers/src/hal_wkio.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hal_wkio$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hal_wkio$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hal_wkio$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hal_wkio$(PreprocessSuffix): ../../../../platform/drivers/src/hal_wkio.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hal_wkio$(PreprocessSuffix) ../../../../platform/drivers/src/hal_wkio.c

$(IntermediateDirectory)/src_main$(ObjectSuffix): ../app/src/main.c  
	$(CC) $(SourceSwitch) ../app/src/main.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_main$(ObjectSuffix) -MF$(IntermediateDirectory)/src_main$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_main$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_main$(PreprocessSuffix): ../app/src/main.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_main$(PreprocessSuffix) ../app/src/main.c

$(IntermediateDirectory)/src_sleep_detect$(ObjectSuffix): ../app/src/sleep_detect.c  
	$(CC) $(SourceSwitch) ../app/src/sleep_detect.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_sleep_detect$(ObjectSuffix) -MF$(IntermediateDirectory)/src_sleep_detect$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_sleep_detect$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_sleep_detect$(PreprocessSuffix): ../app/src/sleep_detect.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_sleep_detect$(PreprocessSuffix) ../app/src/sleep_detect.c

$(IntermediateDirectory)/src_factory_test$(ObjectSuffix): ../../../../subsys/ft/src/factory_test.c  
	$(CC) $(SourceSwitch) ../../../../subsys/ft/src/factory_test.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_factory_test$(ObjectSuffix) -MF$(IntermediateDirectory)/src_factory_test$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_factory_test$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_factory_test$(PreprocessSuffix): ../../../../subsys/ft/src/factory_test.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_factory_test$(PreprocessSuffix) ../../../../subsys/ft/src/factory_test.c

$(IntermediateDirectory)/src_mp_ctrl$(ObjectSuffix): ../../../../subsys/ft/src/mp_ctrl.c  
	$(CC) $(SourceSwitch) ../../../../subsys/ft/src/mp_ctrl.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_mp_ctrl$(ObjectSuffix) -MF$(IntermediateDirectory)/src_mp_ctrl$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_mp_ctrl$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_mp_ctrl$(PreprocessSuffix): ../../../../subsys/ft/src/mp_ctrl.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_mp_ctrl$(PreprocessSuffix) ../../../../subsys/ft/src/mp_ctrl.c

$(IntermediateDirectory)/src_hif$(ObjectSuffix): ../../../../subsys/hif/src/hif.c  
	$(CC) $(SourceSwitch) ../../../../subsys/hif/src/hif.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hif$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hif$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hif$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hif$(PreprocessSuffix): ../../../../subsys/hif/src/hif.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hif$(PreprocessSuffix) ../../../../subsys/hif/src/hif.c

$(IntermediateDirectory)/src_hif_checksum$(ObjectSuffix): ../../../../subsys/hif/src/hif_checksum.c  
	$(CC) $(SourceSwitch) ../../../../subsys/hif/src/hif_checksum.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hif_checksum$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hif_checksum$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hif_checksum$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hif_checksum$(PreprocessSuffix): ../../../../subsys/hif/src/hif_checksum.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hif_checksum$(PreprocessSuffix) ../../../../subsys/hif/src/hif_checksum.c

$(IntermediateDirectory)/src_hif_com$(ObjectSuffix): ../../../../subsys/hif/src/hif_com.c  
	$(CC) $(SourceSwitch) ../../../../subsys/hif/src/hif_com.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hif_com$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hif_com$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hif_com$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hif_com$(PreprocessSuffix): ../../../../subsys/hif/src/hif_com.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hif_com$(PreprocessSuffix) ../../../../subsys/hif/src/hif_com.c

$(IntermediateDirectory)/src_hif_com_dma$(ObjectSuffix): ../../../../subsys/hif/src/hif_com_dma.c  
	$(CC) $(SourceSwitch) ../../../../subsys/hif/src/hif_com_dma.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hif_com_dma$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hif_com_dma$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hif_com_dma$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hif_com_dma$(PreprocessSuffix): ../../../../subsys/hif/src/hif_com_dma.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hif_com_dma$(PreprocessSuffix) ../../../../subsys/hif/src/hif_com_dma.c

$(IntermediateDirectory)/src_hif_com_iic$(ObjectSuffix): ../../../../subsys/hif/src/hif_com_iic.c  
	$(CC) $(SourceSwitch) ../../../../subsys/hif/src/hif_com_iic.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hif_com_iic$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hif_com_iic$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hif_com_iic$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hif_com_iic$(PreprocessSuffix): ../../../../subsys/hif/src/hif_com_iic.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hif_com_iic$(PreprocessSuffix) ../../../../subsys/hif/src/hif_com_iic.c

$(IntermediateDirectory)/src_hif_com_spi$(ObjectSuffix): ../../../../subsys/hif/src/hif_com_spi.c  
	$(CC) $(SourceSwitch) ../../../../subsys/hif/src/hif_com_spi.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hif_com_spi$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hif_com_spi$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hif_com_spi$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hif_com_spi$(PreprocessSuffix): ../../../../subsys/hif/src/hif_com_spi.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hif_com_spi$(PreprocessSuffix) ../../../../subsys/hif/src/hif_com_spi.c

$(IntermediateDirectory)/src_hif_com_uart$(ObjectSuffix): ../../../../subsys/hif/src/hif_com_uart.c  
	$(CC) $(SourceSwitch) ../../../../subsys/hif/src/hif_com_uart.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hif_com_uart$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hif_com_uart$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hif_com_uart$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hif_com_uart$(PreprocessSuffix): ../../../../subsys/hif/src/hif_com_uart.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hif_com_uart$(PreprocessSuffix) ../../../../subsys/hif/src/hif_com_uart.c

$(IntermediateDirectory)/src_hif_io$(ObjectSuffix): ../../../../subsys/hif/src/hif_io.c  
	$(CC) $(SourceSwitch) ../../../../subsys/hif/src/hif_io.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hif_io$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hif_io$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hif_io$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hif_io$(PreprocessSuffix): ../../../../subsys/hif/src/hif_io.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hif_io$(PreprocessSuffix) ../../../../subsys/hif/src/hif_io.c

$(IntermediateDirectory)/src_hif_mem$(ObjectSuffix): ../../../../subsys/hif/src/hif_mem.c  
	$(CC) $(SourceSwitch) ../../../../subsys/hif/src/hif_mem.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hif_mem$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hif_mem$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hif_mem$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hif_mem$(PreprocessSuffix): ../../../../subsys/hif/src/hif_mem.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hif_mem$(PreprocessSuffix) ../../../../subsys/hif/src/hif_mem.c

$(IntermediateDirectory)/src_hif_msg$(ObjectSuffix): ../../../../subsys/hif/src/hif_msg.c  
	$(CC) $(SourceSwitch) ../../../../subsys/hif/src/hif_msg.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hif_msg$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hif_msg$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hif_msg$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hif_msg$(PreprocessSuffix): ../../../../subsys/hif/src/hif_msg.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hif_msg$(PreprocessSuffix) ../../../../subsys/hif/src/hif_msg.c

$(IntermediateDirectory)/src_hif_pm$(ObjectSuffix): ../../../../subsys/hif/src/hif_pm.c  
	$(CC) $(SourceSwitch) ../../../../subsys/hif/src/hif_pm.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_hif_pm$(ObjectSuffix) -MF$(IntermediateDirectory)/src_hif_pm$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_hif_pm$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_hif_pm$(PreprocessSuffix): ../../../../subsys/hif/src/hif_pm.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_hif_pm$(PreprocessSuffix) ../../../../subsys/hif/src/hif_pm.c

$(IntermediateDirectory)/string_strext$(ObjectSuffix): ../../../../subsys/libc/string/strext.c  
	$(CC) $(SourceSwitch) ../../../../subsys/libc/string/strext.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/string_strext$(ObjectSuffix) -MF$(IntermediateDirectory)/string_strext$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/string_strext$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/string_strext$(PreprocessSuffix): ../../../../subsys/libc/string/strext.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/string_strext$(PreprocessSuffix) ../../../../subsys/libc/string/strext.c

$(IntermediateDirectory)/sys_fcntl$(ObjectSuffix): ../../../../subsys/libc/sys/fcntl.c  
	$(CC) $(SourceSwitch) ../../../../subsys/libc/sys/fcntl.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/sys_fcntl$(ObjectSuffix) -MF$(IntermediateDirectory)/sys_fcntl$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/sys_fcntl$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/sys_fcntl$(PreprocessSuffix): ../../../../subsys/libc/sys/fcntl.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/sys_fcntl$(PreprocessSuffix) ../../../../subsys/libc/sys/fcntl.c

$(IntermediateDirectory)/sys_list$(ObjectSuffix): ../../../../subsys/libc/sys/list.c  
	$(CC) $(SourceSwitch) ../../../../subsys/libc/sys/list.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/sys_list$(ObjectSuffix) -MF$(IntermediateDirectory)/sys_list$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/sys_list$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/sys_list$(PreprocessSuffix): ../../../../subsys/libc/sys/list.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/sys_list$(PreprocessSuffix) ../../../../subsys/libc/sys/list.c

$(IntermediateDirectory)/src_log_io$(ObjectSuffix): ../../../../subsys/log/src/log_io.c  
	$(CC) $(SourceSwitch) ../../../../subsys/log/src/log_io.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_log_io$(ObjectSuffix) -MF$(IntermediateDirectory)/src_log_io$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_log_io$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_log_io$(PreprocessSuffix): ../../../../subsys/log/src/log_io.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_log_io$(PreprocessSuffix) ../../../../subsys/log/src/log_io.c

$(IntermediateDirectory)/src_printf_early_port$(ObjectSuffix): ../../../../subsys/log/src/printf_early_port.c  
	$(CC) $(SourceSwitch) ../../../../subsys/log/src/printf_early_port.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_printf_early_port$(ObjectSuffix) -MF$(IntermediateDirectory)/src_printf_early_port$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_printf_early_port$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_printf_early_port$(PreprocessSuffix): ../../../../subsys/log/src/printf_early_port.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_printf_early_port$(PreprocessSuffix) ../../../../subsys/log/src/printf_early_port.c

$(IntermediateDirectory)/src_printf_port$(ObjectSuffix): ../../../../subsys/log/src/printf_port.c  
	$(CC) $(SourceSwitch) ../../../../subsys/log/src/printf_port.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_printf_port$(ObjectSuffix) -MF$(IntermediateDirectory)/src_printf_port$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_printf_port$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_printf_port$(PreprocessSuffix): ../../../../subsys/log/src/printf_port.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_printf_port$(PreprocessSuffix) ../../../../subsys/log/src/printf_port.c

$(IntermediateDirectory)/mmw_algorithm_cfar_casogo$(ObjectSuffix): ../../../../subsys/mmw/mmw_algorithm/cfar_casogo.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_algorithm/cfar_casogo.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/mmw_algorithm_cfar_casogo$(ObjectSuffix) -MF$(IntermediateDirectory)/mmw_algorithm_cfar_casogo$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/mmw_algorithm_cfar_casogo$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/mmw_algorithm_cfar_casogo$(PreprocessSuffix): ../../../../subsys/mmw/mmw_algorithm/cfar_casogo.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/mmw_algorithm_cfar_casogo$(PreprocessSuffix) ../../../../subsys/mmw/mmw_algorithm/cfar_casogo.c

$(IntermediateDirectory)/mmw_algorithm_mmw_alg_ant_calibration$(ObjectSuffix): ../../../../subsys/mmw/mmw_algorithm/mmw_alg_ant_calibration.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_algorithm/mmw_alg_ant_calibration.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/mmw_algorithm_mmw_alg_ant_calibration$(ObjectSuffix) -MF$(IntermediateDirectory)/mmw_algorithm_mmw_alg_ant_calibration$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/mmw_algorithm_mmw_alg_ant_calibration$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/mmw_algorithm_mmw_alg_ant_calibration$(PreprocessSuffix): ../../../../subsys/mmw/mmw_algorithm/mmw_alg_ant_calibration.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/mmw_algorithm_mmw_alg_ant_calibration$(PreprocessSuffix) ../../../../subsys/mmw/mmw_algorithm/mmw_alg_ant_calibration.c

$(IntermediateDirectory)/mmw_algorithm_mmw_alg_debug$(ObjectSuffix): ../../../../subsys/mmw/mmw_algorithm/mmw_alg_debug.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_algorithm/mmw_alg_debug.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/mmw_algorithm_mmw_alg_debug$(ObjectSuffix) -MF$(IntermediateDirectory)/mmw_algorithm_mmw_alg_debug$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/mmw_algorithm_mmw_alg_debug$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/mmw_algorithm_mmw_alg_debug$(PreprocessSuffix): ../../../../subsys/mmw/mmw_algorithm/mmw_alg_debug.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/mmw_algorithm_mmw_alg_debug$(PreprocessSuffix) ../../../../subsys/mmw/mmw_algorithm/mmw_alg_debug.c

$(IntermediateDirectory)/mmw_algorithm_mmw_alg_doa$(ObjectSuffix): ../../../../subsys/mmw/mmw_algorithm/mmw_alg_doa.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_algorithm/mmw_alg_doa.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/mmw_algorithm_mmw_alg_doa$(ObjectSuffix) -MF$(IntermediateDirectory)/mmw_algorithm_mmw_alg_doa$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/mmw_algorithm_mmw_alg_doa$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/mmw_algorithm_mmw_alg_doa$(PreprocessSuffix): ../../../../subsys/mmw/mmw_algorithm/mmw_alg_doa.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/mmw_algorithm_mmw_alg_doa$(PreprocessSuffix) ../../../../subsys/mmw/mmw_algorithm/mmw_alg_doa.c

$(IntermediateDirectory)/mmw_algorithm_mmw_alg_micro_cube$(ObjectSuffix): ../../../../subsys/mmw/mmw_algorithm/mmw_alg_micro_cube.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_algorithm/mmw_alg_micro_cube.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/mmw_algorithm_mmw_alg_micro_cube$(ObjectSuffix) -MF$(IntermediateDirectory)/mmw_algorithm_mmw_alg_micro_cube$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/mmw_algorithm_mmw_alg_micro_cube$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/mmw_algorithm_mmw_alg_micro_cube$(PreprocessSuffix): ../../../../subsys/mmw/mmw_algorithm/mmw_alg_micro_cube.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/mmw_algorithm_mmw_alg_micro_cube$(PreprocessSuffix) ../../../../subsys/mmw/mmw_algorithm/mmw_alg_micro_cube.c

$(IntermediateDirectory)/mmw_algorithm_mmw_alg_pointcloud$(ObjectSuffix): ../../../../subsys/mmw/mmw_algorithm/mmw_alg_pointcloud.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_algorithm/mmw_alg_pointcloud.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/mmw_algorithm_mmw_alg_pointcloud$(ObjectSuffix) -MF$(IntermediateDirectory)/mmw_algorithm_mmw_alg_pointcloud$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/mmw_algorithm_mmw_alg_pointcloud$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/mmw_algorithm_mmw_alg_pointcloud$(PreprocessSuffix): ../../../../subsys/mmw/mmw_algorithm/mmw_alg_pointcloud.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/mmw_algorithm_mmw_alg_pointcloud$(PreprocessSuffix) ../../../../subsys/mmw/mmw_algorithm/mmw_alg_pointcloud.c

$(IntermediateDirectory)/mmw_application_mmw_app_micro_pointcloud$(ObjectSuffix): ../../../../subsys/mmw/mmw_application/mmw_app_micro_pointcloud.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_application/mmw_app_micro_pointcloud.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/mmw_application_mmw_app_micro_pointcloud$(ObjectSuffix) -MF$(IntermediateDirectory)/mmw_application_mmw_app_micro_pointcloud$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/mmw_application_mmw_app_micro_pointcloud$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/mmw_application_mmw_app_micro_pointcloud$(PreprocessSuffix): ../../../../subsys/mmw/mmw_application/mmw_app_micro_pointcloud.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/mmw_application_mmw_app_micro_pointcloud$(PreprocessSuffix) ../../../../subsys/mmw/mmw_application/mmw_app_micro_pointcloud.c

$(IntermediateDirectory)/mmw_application_mmw_app_pointcloud$(ObjectSuffix): ../../../../subsys/mmw/mmw_application/mmw_app_pointcloud.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_application/mmw_app_pointcloud.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/mmw_application_mmw_app_pointcloud$(ObjectSuffix) -MF$(IntermediateDirectory)/mmw_application_mmw_app_pointcloud$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/mmw_application_mmw_app_pointcloud$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/mmw_application_mmw_app_pointcloud$(PreprocessSuffix): ../../../../subsys/mmw/mmw_application/mmw_app_pointcloud.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/mmw_application_mmw_app_pointcloud$(PreprocessSuffix) ../../../../subsys/mmw/mmw_application/mmw_app_pointcloud.c

$(IntermediateDirectory)/mmw_application_mmw_app_pointcloud_config$(ObjectSuffix): ../../../../subsys/mmw/mmw_application/mmw_app_pointcloud_config.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_application/mmw_app_pointcloud_config.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/mmw_application_mmw_app_pointcloud_config$(ObjectSuffix) -MF$(IntermediateDirectory)/mmw_application_mmw_app_pointcloud_config$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/mmw_application_mmw_app_pointcloud_config$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/mmw_application_mmw_app_pointcloud_config$(PreprocessSuffix): ../../../../subsys/mmw/mmw_application/mmw_app_pointcloud_config.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/mmw_application_mmw_app_pointcloud_config$(PreprocessSuffix) ../../../../subsys/mmw/mmw_application/mmw_app_pointcloud_config.c

$(IntermediateDirectory)/mmw_mdsp_fast_func$(ObjectSuffix): ../../../../subsys/mmw/mmw_mdsp/fast_func.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_mdsp/fast_func.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/mmw_mdsp_fast_func$(ObjectSuffix) -MF$(IntermediateDirectory)/mmw_mdsp_fast_func$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/mmw_mdsp_fast_func$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/mmw_mdsp_fast_func$(PreprocessSuffix): ../../../../subsys/mmw/mmw_mdsp/fast_func.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/mmw_mdsp_fast_func$(PreprocessSuffix) ../../../../subsys/mmw/mmw_mdsp/fast_func.c

$(IntermediateDirectory)/mmw_mdsp_fft_func$(ObjectSuffix): ../../../../subsys/mmw/mmw_mdsp/fft_func.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_mdsp/fft_func.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/mmw_mdsp_fft_func$(ObjectSuffix) -MF$(IntermediateDirectory)/mmw_mdsp_fft_func$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/mmw_mdsp_fft_func$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/mmw_mdsp_fft_func$(PreprocessSuffix): ../../../../subsys/mmw/mmw_mdsp/fft_func.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/mmw_mdsp_fft_func$(PreprocessSuffix) ../../../../subsys/mmw/mmw_mdsp/fft_func.c

$(IntermediateDirectory)/mmw_mdsp_HwAux$(ObjectSuffix): ../../../../subsys/mmw/mmw_mdsp/HwAux.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_mdsp/HwAux.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/mmw_mdsp_HwAux$(ObjectSuffix) -MF$(IntermediateDirectory)/mmw_mdsp_HwAux$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/mmw_mdsp_HwAux$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/mmw_mdsp_HwAux$(PreprocessSuffix): ../../../../subsys/mmw/mmw_mdsp/HwAux.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/mmw_mdsp_HwAux$(PreprocessSuffix) ../../../../subsys/mmw/mmw_mdsp/HwAux.c

$(IntermediateDirectory)/mmw_mdsp_Luts$(ObjectSuffix): ../../../../subsys/mmw/mmw_mdsp/Luts.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_mdsp/Luts.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/mmw_mdsp_Luts$(ObjectSuffix) -MF$(IntermediateDirectory)/mmw_mdsp_Luts$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/mmw_mdsp_Luts$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/mmw_mdsp_Luts$(PreprocessSuffix): ../../../../subsys/mmw/mmw_mdsp/Luts.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/mmw_mdsp_Luts$(PreprocessSuffix) ../../../../subsys/mmw/mmw_mdsp/Luts.c

$(IntermediateDirectory)/port_port$(ObjectSuffix): ../../../../subsys/pm/port/port.c  
	$(CC) $(SourceSwitch) ../../../../subsys/pm/port/port.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/port_port$(ObjectSuffix) -MF$(IntermediateDirectory)/port_port$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/port_port$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/port_port$(PreprocessSuffix): ../../../../subsys/pm/port/port.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/port_port$(PreprocessSuffix) ../../../../subsys/pm/port/port.c

$(IntermediateDirectory)/src_mtest$(ObjectSuffix): ../../../../subsys/testsuite/src/mtest.c  
	$(CC) $(SourceSwitch) ../../../../subsys/testsuite/src/mtest.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_mtest$(ObjectSuffix) -MF$(IntermediateDirectory)/src_mtest$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_mtest$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_mtest$(PreprocessSuffix): ../../../../subsys/testsuite/src/mtest.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_mtest$(PreprocessSuffix) ../../../../subsys/testsuite/src/mtest.c

$(IntermediateDirectory)/src_sleep$(ObjectSuffix): ../../../../platform/arch/riscv/src/sleep.S  
	$(AS) $(SourceSwitch) ../../../../platform/arch/riscv/src/sleep.S $(ASFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_sleep$(ObjectSuffix) -MF$(IntermediateDirectory)/src_sleep$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_sleep$(ObjectSuffix) $(IncludeAPath) $(IncludePackagePath)
Lst/src_sleep$(PreprocessSuffix): ../../../../platform/arch/riscv/src/sleep.S
	$(CC) $(CFLAGS)$(IncludeAPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_sleep$(PreprocessSuffix) ../../../../platform/arch/riscv/src/sleep.S

$(IntermediateDirectory)/src_startup$(ObjectSuffix): ../../../../platform/arch/riscv/src/startup.S  
	$(AS) $(SourceSwitch) ../../../../platform/arch/riscv/src/startup.S $(ASFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_startup$(ObjectSuffix) -MF$(IntermediateDirectory)/src_startup$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_startup$(ObjectSuffix) $(IncludeAPath) $(IncludePackagePath)
Lst/src_startup$(PreprocessSuffix): ../../../../platform/arch/riscv/src/startup.S
	$(CC) $(CFLAGS)$(IncludeAPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_startup$(PreprocessSuffix) ../../../../platform/arch/riscv/src/startup.S

$(IntermediateDirectory)/src_startup_c$(ObjectSuffix): ../../../../platform/arch/riscv/src/startup_c.c  
	$(CC) $(SourceSwitch) ../../../../platform/arch/riscv/src/startup_c.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_startup_c$(ObjectSuffix) -MF$(IntermediateDirectory)/src_startup_c$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_startup_c$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_startup_c$(PreprocessSuffix): ../../../../platform/arch/riscv/src/startup_c.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_startup_c$(PreprocessSuffix) ../../../../platform/arch/riscv/src/startup_c.c

$(IntermediateDirectory)/src_trap$(ObjectSuffix): ../../../../platform/arch/riscv/src/trap.S  
	$(AS) $(SourceSwitch) ../../../../platform/arch/riscv/src/trap.S $(ASFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_trap$(ObjectSuffix) -MF$(IntermediateDirectory)/src_trap$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_trap$(ObjectSuffix) $(IncludeAPath) $(IncludePackagePath)
Lst/src_trap$(PreprocessSuffix): ../../../../platform/arch/riscv/src/trap.S
	$(CC) $(CFLAGS)$(IncludeAPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_trap$(PreprocessSuffix) ../../../../platform/arch/riscv/src/trap.S

$(IntermediateDirectory)/src_trap_c$(ObjectSuffix): ../../../../platform/arch/riscv/src/trap_c.c  
	$(CC) $(SourceSwitch) ../../../../platform/arch/riscv/src/trap_c.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_trap_c$(ObjectSuffix) -MF$(IntermediateDirectory)/src_trap_c$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_trap_c$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_trap_c$(PreprocessSuffix): ../../../../platform/arch/riscv/src/trap_c.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_trap_c$(PreprocessSuffix) ../../../../platform/arch/riscv/src/trap_c.c

$(IntermediateDirectory)/src_vectors$(ObjectSuffix): ../../../../platform/arch/riscv/src/vectors.S  
	$(AS) $(SourceSwitch) ../../../../platform/arch/riscv/src/vectors.S $(ASFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_vectors$(ObjectSuffix) -MF$(IntermediateDirectory)/src_vectors$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_vectors$(ObjectSuffix) $(IncludeAPath) $(IncludePackagePath)
Lst/src_vectors$(PreprocessSuffix): ../../../../platform/arch/riscv/src/vectors.S
	$(CC) $(CFLAGS)$(IncludeAPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_vectors$(PreprocessSuffix) ../../../../platform/arch/riscv/src/vectors.S

$(IntermediateDirectory)/src_irq$(ObjectSuffix): ../../../../platform/soc/common/src/irq.c  
	$(CC) $(SourceSwitch) ../../../../platform/soc/common/src/irq.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_irq$(ObjectSuffix) -MF$(IntermediateDirectory)/src_irq$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_irq$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_irq$(PreprocessSuffix): ../../../../platform/soc/common/src/irq.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_irq$(PreprocessSuffix) ../../../../platform/soc/common/src/irq.c

$(IntermediateDirectory)/src_pmu_tmr$(ObjectSuffix): ../../../../platform/soc/common/src/pmu_tmr.c  
	$(CC) $(SourceSwitch) ../../../../platform/soc/common/src/pmu_tmr.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_pmu_tmr$(ObjectSuffix) -MF$(IntermediateDirectory)/src_pmu_tmr$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_pmu_tmr$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_pmu_tmr$(PreprocessSuffix): ../../../../platform/soc/common/src/pmu_tmr.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_pmu_tmr$(PreprocessSuffix) ../../../../platform/soc/common/src/pmu_tmr.c

$(IntermediateDirectory)/src_soc$(ObjectSuffix): ../../../../platform/soc/common/src/soc.c  
	$(CC) $(SourceSwitch) ../../../../platform/soc/common/src/soc.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_soc$(ObjectSuffix) -MF$(IntermediateDirectory)/src_soc$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_soc$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_soc$(PreprocessSuffix): ../../../../platform/soc/common/src/soc.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_soc$(PreprocessSuffix) ../../../../platform/soc/common/src/soc.c

$(IntermediateDirectory)/src_rs613x$(ObjectSuffix): ../../../../platform/soc/rs613x/src/rs613x.c  
	$(CC) $(SourceSwitch) ../../../../platform/soc/rs613x/src/rs613x.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_rs613x$(ObjectSuffix) -MF$(IntermediateDirectory)/src_rs613x$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_rs613x$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_rs613x$(PreprocessSuffix): ../../../../platform/soc/rs613x/src/rs613x.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_rs613x$(PreprocessSuffix) ../../../../platform/soc/rs613x/src/rs613x.c

$(IntermediateDirectory)/src_rs624x$(ObjectSuffix): ../../../../platform/soc/rs624x/src/rs624x.c  
	$(CC) $(SourceSwitch) ../../../../platform/soc/rs624x/src/rs624x.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_rs624x$(ObjectSuffix) -MF$(IntermediateDirectory)/src_rs624x$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_rs624x$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_rs624x$(PreprocessSuffix): ../../../../platform/soc/rs624x/src/rs624x.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_rs624x$(PreprocessSuffix) ../../../../platform/soc/rs624x/src/rs624x.c

$(IntermediateDirectory)/src_kvf$(ObjectSuffix): ../../../../subsys/kv/kvf/src/kvf.c  
	$(CC) $(SourceSwitch) ../../../../subsys/kv/kvf/src/kvf.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_kvf$(ObjectSuffix) -MF$(IntermediateDirectory)/src_kvf$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_kvf$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_kvf$(PreprocessSuffix): ../../../../subsys/kv/kvf/src/kvf.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_kvf$(PreprocessSuffix) ../../../../subsys/kv/kvf/src/kvf.c

$(IntermediateDirectory)/src_nvs$(ObjectSuffix): ../../../../subsys/kv/kvf/src/nvs.c  
	$(CC) $(SourceSwitch) ../../../../subsys/kv/kvf/src/nvs.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_nvs$(ObjectSuffix) -MF$(IntermediateDirectory)/src_nvs$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_nvs$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_nvs$(PreprocessSuffix): ../../../../subsys/kv/kvf/src/nvs.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_nvs$(PreprocessSuffix) ../../../../subsys/kv/kvf/src/nvs.c

$(IntermediateDirectory)/src_kvr$(ObjectSuffix): ../../../../subsys/kv/kvr/src/kvr.c  
	$(CC) $(SourceSwitch) ../../../../subsys/kv/kvr/src/kvr.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_kvr$(ObjectSuffix) -MF$(IntermediateDirectory)/src_kvr$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_kvr$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_kvr$(PreprocessSuffix): ../../../../subsys/kv/kvr/src/kvr.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_kvr$(PreprocessSuffix) ../../../../subsys/kv/kvr/src/kvr.c

$(IntermediateDirectory)/test_mmw_point_cloud_ut$(ObjectSuffix): ../../../../subsys/mmw/mmw_application/test/mmw_point_cloud_ut.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_application/test/mmw_point_cloud_ut.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/test_mmw_point_cloud_ut$(ObjectSuffix) -MF$(IntermediateDirectory)/test_mmw_point_cloud_ut$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/test_mmw_point_cloud_ut$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/test_mmw_point_cloud_ut$(PreprocessSuffix): ../../../../subsys/mmw/mmw_application/test/mmw_point_cloud_ut.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/test_mmw_point_cloud_ut$(PreprocessSuffix) ../../../../subsys/mmw/mmw_application/test/mmw_point_cloud_ut.c

$(IntermediateDirectory)/test_mmw_point_cloud_vv$(ObjectSuffix): ../../../../subsys/mmw/mmw_application/test/mmw_point_cloud_vv.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_application/test/mmw_point_cloud_vv.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/test_mmw_point_cloud_vv$(ObjectSuffix) -MF$(IntermediateDirectory)/test_mmw_point_cloud_vv$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/test_mmw_point_cloud_vv$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/test_mmw_point_cloud_vv$(PreprocessSuffix): ../../../../subsys/mmw/mmw_application/test/mmw_point_cloud_vv.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/test_mmw_point_cloud_vv$(PreprocessSuffix) ../../../../subsys/mmw/mmw_application/test/mmw_point_cloud_vv.c

$(IntermediateDirectory)/src_mmw_hif$(ObjectSuffix): ../../../../subsys/mmw/mmw_cmd/src/mmw_hif.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_cmd/src/mmw_hif.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_mmw_hif$(ObjectSuffix) -MF$(IntermediateDirectory)/src_mmw_hif$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_mmw_hif$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_mmw_hif$(PreprocessSuffix): ../../../../subsys/mmw/mmw_cmd/src/mmw_hif.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_mmw_hif$(PreprocessSuffix) ../../../../subsys/mmw/mmw_cmd/src/mmw_hif.c

$(IntermediateDirectory)/src_mmw_report$(ObjectSuffix): ../../../../subsys/mmw/mmw_cmd_sleep/src/mmw_report.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_cmd_sleep/src/mmw_report.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_mmw_report$(ObjectSuffix) -MF$(IntermediateDirectory)/src_mmw_report$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_mmw_report$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_mmw_report$(PreprocessSuffix): ../../../../subsys/mmw/mmw_cmd_sleep/src/mmw_report.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_mmw_report$(PreprocessSuffix) ../../../../subsys/mmw/mmw_cmd_sleep/src/mmw_report.c

$(IntermediateDirectory)/src_mmw_shell$(ObjectSuffix): ../../../../subsys/mmw/mmw_cmd/src/mmw_shell.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_cmd/src/mmw_shell.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_mmw_shell$(ObjectSuffix) -MF$(IntermediateDirectory)/src_mmw_shell$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_mmw_shell$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_mmw_shell$(PreprocessSuffix): ../../../../subsys/mmw/mmw_cmd/src/mmw_shell.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_mmw_shell$(PreprocessSuffix) ../../../../subsys/mmw/mmw_cmd/src/mmw_shell.c

$(IntermediateDirectory)/src_mmw_ctrl$(ObjectSuffix): ../../../../subsys/mmw/mmw_ctrl/src/mmw_ctrl.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_ctrl/src/mmw_ctrl.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_mmw_ctrl$(ObjectSuffix) -MF$(IntermediateDirectory)/src_mmw_ctrl$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_mmw_ctrl$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_mmw_ctrl$(PreprocessSuffix): ../../../../subsys/mmw/mmw_ctrl/src/mmw_ctrl.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_mmw_ctrl$(PreprocessSuffix) ../../../../subsys/mmw/mmw_ctrl/src/mmw_ctrl.c

$(IntermediateDirectory)/src_mmw_dev$(ObjectSuffix): ../../../../subsys/mmw/mmw_ctrl/src/mmw_dev.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_ctrl/src/mmw_dev.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_mmw_dev$(ObjectSuffix) -MF$(IntermediateDirectory)/src_mmw_dev$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_mmw_dev$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_mmw_dev$(PreprocessSuffix): ../../../../subsys/mmw/mmw_ctrl/src/mmw_dev.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_mmw_dev$(PreprocessSuffix) ../../../../subsys/mmw/mmw_ctrl/src/mmw_dev.c

$(IntermediateDirectory)/src_mmw_dev_shell$(ObjectSuffix): ../../../../subsys/mmw/mmw_ctrl/src/mmw_dev_shell.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_ctrl/src/mmw_dev_shell.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_mmw_dev_shell$(ObjectSuffix) -MF$(IntermediateDirectory)/src_mmw_dev_shell$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_mmw_dev_shell$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_mmw_dev_shell$(PreprocessSuffix): ../../../../subsys/mmw/mmw_ctrl/src/mmw_dev_shell.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_mmw_dev_shell$(PreprocessSuffix) ../../../../subsys/mmw/mmw_ctrl/src/mmw_dev_shell.c

$(IntermediateDirectory)/src_mmw_low_power$(ObjectSuffix): ../../../../subsys/mmw/mmw_ctrl/src/mmw_low_power.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_ctrl/src/mmw_low_power.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_mmw_low_power$(ObjectSuffix) -MF$(IntermediateDirectory)/src_mmw_low_power$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_mmw_low_power$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_mmw_low_power$(PreprocessSuffix): ../../../../subsys/mmw/mmw_ctrl/src/mmw_low_power.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_mmw_low_power$(PreprocessSuffix) ../../../../subsys/mmw/mmw_ctrl/src/mmw_low_power.c

$(IntermediateDirectory)/src_mmw_pal$(ObjectSuffix): ../../../../subsys/mmw/mmw_ctrl/src/mmw_pal.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_ctrl/src/mmw_pal.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_mmw_pal$(ObjectSuffix) -MF$(IntermediateDirectory)/src_mmw_pal$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_mmw_pal$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_mmw_pal$(PreprocessSuffix): ../../../../subsys/mmw/mmw_ctrl/src/mmw_pal.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_mmw_pal$(PreprocessSuffix) ../../../../subsys/mmw/mmw_ctrl/src/mmw_pal.c

$(IntermediateDirectory)/src_mmw_temp_mgmt$(ObjectSuffix): ../../../../subsys/mmw/mmw_ctrl/src/mmw_temp_mgmt.c  
	$(CC) $(SourceSwitch) ../../../../subsys/mmw/mmw_ctrl/src/mmw_temp_mgmt.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_mmw_temp_mgmt$(ObjectSuffix) -MF$(IntermediateDirectory)/src_mmw_temp_mgmt$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_mmw_temp_mgmt$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_mmw_temp_mgmt$(PreprocessSuffix): ../../../../subsys/mmw/mmw_ctrl/src/mmw_temp_mgmt.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_mmw_temp_mgmt$(PreprocessSuffix) ../../../../subsys/mmw/mmw_ctrl/src/mmw_temp_mgmt.c

$(IntermediateDirectory)/src_shell$(ObjectSuffix): ../../../../subsys/shell/letter_shell/src/shell.c  
	$(CC) $(SourceSwitch) ../../../../subsys/shell/letter_shell/src/shell.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_shell$(ObjectSuffix) -MF$(IntermediateDirectory)/src_shell$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_shell$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_shell$(PreprocessSuffix): ../../../../subsys/shell/letter_shell/src/shell.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_shell$(PreprocessSuffix) ../../../../subsys/shell/letter_shell/src/shell.c

$(IntermediateDirectory)/src_shell_cmd_group$(ObjectSuffix): ../../../../subsys/shell/letter_shell/src/shell_cmd_group.c  
	$(CC) $(SourceSwitch) ../../../../subsys/shell/letter_shell/src/shell_cmd_group.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_shell_cmd_group$(ObjectSuffix) -MF$(IntermediateDirectory)/src_shell_cmd_group$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_shell_cmd_group$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_shell_cmd_group$(PreprocessSuffix): ../../../../subsys/shell/letter_shell/src/shell_cmd_group.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_shell_cmd_group$(PreprocessSuffix) ../../../../subsys/shell/letter_shell/src/shell_cmd_group.c

$(IntermediateDirectory)/src_shell_cmd_list$(ObjectSuffix): ../../../../subsys/shell/letter_shell/src/shell_cmd_list.c  
	$(CC) $(SourceSwitch) ../../../../subsys/shell/letter_shell/src/shell_cmd_list.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_shell_cmd_list$(ObjectSuffix) -MF$(IntermediateDirectory)/src_shell_cmd_list$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_shell_cmd_list$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_shell_cmd_list$(PreprocessSuffix): ../../../../subsys/shell/letter_shell/src/shell_cmd_list.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_shell_cmd_list$(PreprocessSuffix) ../../../../subsys/shell/letter_shell/src/shell_cmd_list.c

$(IntermediateDirectory)/src_shell_companion$(ObjectSuffix): ../../../../subsys/shell/letter_shell/src/shell_companion.c  
	$(CC) $(SourceSwitch) ../../../../subsys/shell/letter_shell/src/shell_companion.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_shell_companion$(ObjectSuffix) -MF$(IntermediateDirectory)/src_shell_companion$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_shell_companion$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_shell_companion$(PreprocessSuffix): ../../../../subsys/shell/letter_shell/src/shell_companion.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_shell_companion$(PreprocessSuffix) ../../../../subsys/shell/letter_shell/src/shell_companion.c

$(IntermediateDirectory)/src_shell_ext$(ObjectSuffix): ../../../../subsys/shell/letter_shell/src/shell_ext.c  
	$(CC) $(SourceSwitch) ../../../../subsys/shell/letter_shell/src/shell_ext.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_shell_ext$(ObjectSuffix) -MF$(IntermediateDirectory)/src_shell_ext$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_shell_ext$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_shell_ext$(PreprocessSuffix): ../../../../subsys/shell/letter_shell/src/shell_ext.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_shell_ext$(PreprocessSuffix) ../../../../subsys/shell/letter_shell/src/shell_ext.c

$(IntermediateDirectory)/src_shell_port$(ObjectSuffix): ../../../../subsys/shell/letter_shell/src/shell_port.c  
	$(CC) $(SourceSwitch) ../../../../subsys/shell/letter_shell/src/shell_port.c $(CFLAGS) -MMD -MP -MT$(IntermediateDirectory)/src_shell_port$(ObjectSuffix) -MF$(IntermediateDirectory)/src_shell_port$(DependSuffix) $(ObjectSwitch)$(IntermediateDirectory)/src_shell_port$(ObjectSuffix) $(IncludeCPath) $(IncludePackagePath)
Lst/src_shell_port$(PreprocessSuffix): ../../../../subsys/shell/letter_shell/src/shell_port.c
	$(CC) $(CFLAGS)$(IncludeCPath) $(PreprocessOnlySwitch) $(OutputSwitch) Lst/src_shell_port$(PreprocessSuffix) ../../../../subsys/shell/letter_shell/src/shell_port.c


-include $(IntermediateDirectory)/*$(DependSuffix)

# Stack evidence for the changed diagnostic sources; no clean/delete stage.
CFLAGS += -fstack-usage

$(IntermediateDirectory)/src_heart_rate_core$(ObjectSuffix): ../app/src/heart_rate_core.c
	$(CC) $(SourceSwitch) ../app/src/heart_rate_core.c $(CFLAGS) -ffp-contract=off -fstack-usage -MMD -MP -MT$@ -MF$(IntermediateDirectory)/src_heart_rate_core$(DependSuffix) $(ObjectSwitch)$@ $(IncludeCPath) $(IncludePackagePath)

$(IntermediateDirectory)/src_v6_heart_device$(ObjectSuffix): ../app/src/v6_heart_device.c
	$(CC) $(SourceSwitch) ../app/src/v6_heart_device.c $(CFLAGS) -fstack-usage -MMD -MP -MT$@ -MF$(IntermediateDirectory)/src_v6_heart_device$(DependSuffix) $(ObjectSwitch)$@ $(IncludeCPath) $(IncludePackagePath)
