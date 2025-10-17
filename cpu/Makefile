# Thanks to Job Vranish (https://spin.atomicobject.com/2016/08/26/makefile-c-projects/)

APP ?= demo
TARGET_EXEC   := cpu.elf
TARGET_BIN    := cpu.bin

# Directories
BUILD_DIR       := ./build
ASM_DIR         := $(BUILD_DIR)/asm
COMMON_DIR      := ../common
SRC_DIR         := ./src
LIB_DIR         := ./lib
APP_DIR         ?= $(SRC_DIR)/apps/$(APP)
# APP_COMMON_DIR  := $(SRC_DIR)

# Toolchain
PREFIX := arm-none-eabi-
CC     :=${PREFIX}gcc
CXX    :=${PREFIX}gcc
LD     :=${PREFIX}ld
BIN    :=$(PREFIX)objcopy

# Libraries & Includes
LIB_C    ?= /usr/arm-none-eabi/lib
LIB_GCC  ?= /lib/gcc/arm-none-eabi/14.2.1/
NEWLIB ?= /usr/arm-none-eabi/include/

LPATH := -L"${LIB_C}" -L"${LIB_GCC}"

INC_DIRS := $(NEWLIB)
INC_DIRS += $(LIB_GCC)include/
INC_DIRS += $(shell find $(SRC_DIR) -type d -not -path "$(SRC_DIR)/apps/*" )
INC_DIRS += $(shell find $(COMMON_DIR) -type d)
INC_DIRS += $(shell find $(LIB_DIR) -type d -not -path "$(SRC_DIR)/LEAF/*" )
INC_DIRS += $(shell find $(LIB_DIR)/LEAF/leaf/ -type d)
INC_DIRS += $(shell find $(LIB_DIR)/FreeRTOS-Kernel/include/ -type d)
INC_DIRS += $(shell find $(LIB_DIR)/FreeRTOS-Kernel/portable/GCC/AM1802/ -type d)
INC_DIRS += $(shell find $(LIB_DIR)/am18x-lib/ -type d)
INC_DIRS += $(shell find $(LIB_DIR)/am18x-lib/inc/ -type d)
INC_DIRS += $(shell find $(APP_DIR) -type d)

# Add a prefix to INC_DIRS. So moduleA would become -ImoduleA. GCC understands this -I flag.
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

# -------------------------------------------------------------------
# Compiler Flags
# -------------------------------------------------------------------

NEST_INT ?= 0
OPTIMISE ?= -Os
CPU      ?= arm926ej-s
CFLAGS   := -mcpu=$(CPU) $(OPTIMISE)  -fdata-sections -ffunction-sections -fanalyzer -fstack-usage -Wstack-usage=192 -Wall -Wno-unused
CFLAGS   += -DDEBUG -DDEBUG_SDDRIVER
CFLAGS   += -Wno-strict-aliasing

ASM_FLAGS := -x assembler-with-cpp
CPPFLAGS := $(INC_FLAGS) -MMD -MP -DNESTED_INTERRUPTS=${NEST_INT}

LDFLAGS := -e start -u start -u __aeabi_uidiv -u __aeabi_idiv $(LPATH) -lm -lc -lgcc  \
					 -T cpu.lds -Map $(BUILD_DIR)/boot.map \
					 --gc-sections --print-memory-usage --no-warn-rwx-segments 

BIN_FLAGS := -O binary

# -------------------------------------------------------------------
# Source Files
# -------------------------------------------------------------------

SRCS := $(shell find $(SRC_DIR)  \( -name '*.cpp' -or -name '*.c' -or -name '*.S' \) \
		-not -path "$(SRC_DIR)/apps/*")
SRCS += $(shell find $(COMMON_DIR) -name '*.c' -o -name '*.cpp' -o -name '*.S')
SRCS += $(shell find $(LIB_DIR)  \( -name '*.cpp' -or -name '*.c' -or -name '*.S' \) \
		-not -path "$(LIB_DIR)/tinyusb/*" \
		-not -path "$(LIB_DIR)/FreeRTOS-Kernel/*" \
		-not -path "$(LIB_DIR)/LEAF/*")
SRCS += $(shell find $(LIB_DIR)/LEAF/leaf/  -name '*.cpp' -or -name '*.c' -or -name '*.S')
SRCS += $(wildcard $(LIB_DIR)/FreeRTOS-Kernel/portable/GCC/AM1802/*.c)
SRCS += $(wildcard $(LIB_DIR)/FreeRTOS-Kernel/*.c)
SRCS += $(shell find $(APP_DIR)  -name '*.cpp' -or -name '*.c' -or -name '*.S')

# Objects and dependencies
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

# Assembly files (for inspection)
ASMS := $(filter-out %.S, $(SRCS))
ASMS := $(ASMS:%=$(ASM_DIR)/%.s)


# Extract binary from ELF.
$(BUILD_DIR)/$(TARGET_BIN): $(BUILD_DIR)/$(TARGET_EXEC)
	$(BIN) $(BIN_FLAGS) $(BUILD_DIR)/$(TARGET_EXEC) $(BUILD_DIR)/$(TARGET_BIN)

# Link.
$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(LD) $(OBJS) -o $@ $(LDFLAGS)

# Build step ASM source.
$(BUILD_DIR)/%.S.o: %.S
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ASM_FLAGS) -c $< -o $@

# Build step for C source.
$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Build step for C++ source.
$(BUILD_DIR)/%.cpp.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Emit compiler-generated assembly for inspection
$(ASM_DIR)/%.c.s: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -S -fverbose-asm $< -o $@

$(ASM_DIR)/%.cpp.s: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -S -fverbose-asm $< -o $@

# -------------------------------------------------------------------
# Phony targets
# -------------------------------------------------------------------

.PHONY: clean asm
clean:
	rm -rf $(BUILD_DIR)

asm: $(ASMS)

# Include the .d makefiles. The - at the front suppresses
# the errors of missing Makefiles. 
# Initially, all the .d files will be missing, 
# and we don't want those errors to show up.
-include $(DEPS) 
