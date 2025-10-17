# Thanks to Job Vranish (https://spin.atomicobject.com/2016/08/26/makefile-c-projects/)

MODULE ?= default 
TARGET_EXEC    := bfin.elf
TARGET_BIN     := bfin.ldr

# Directories
BUILD_DIR       := ./build
ASM_DIR         := $(BUILD_DIR)/asm
COMMON_DIR      := ../common
SRC_DIR         := ./src
LIB_DIR         := ./lib
MODULE_DIR      ?= $(SRC_DIR)/modules/$(MODULE)

# Toolchain
BFIN_TOOLCHAIN ?= /opt/bfin-elf/bin/
BFIN_LIB       := /opt/bfin-elf/bfin-elf/include/
PREFIX         := bfin-elf-
CC             := $(BFIN_TOOLCHAIN)${PREFIX}gcc
CXX            := $(BFIN_TOOLCHAIN)${PREFIX}gcc
LDR            := $(BFIN_TOOLCHAIN)$(PREFIX)ldr
LD             := $(BFIN_TOOLCHAIN)${PREFIX}ld
BIN            := $(BFIN_TOOLCHAIN)$(PREFIX)objcopy

# Libraries and include dirs
INC_DIRS := $(BFIN_LIB)
INC_DIRS += $(shell find $(SRC_DIR) -type d -not -path "$(SRC_DIR)/modules/*" )
INC_DIRS += $(shell find $(COMMON_DIR) -type d)
INC_DIRS += $(shell find $(LIB_DIR) -type d -not -path "$(LIB_DIR)/aleph-dsp/lib/libfixmath/*" )
INC_DIRS += $(shell find $(LIB_DIR)/aleph-dsp/lib/libfixmath/libfixmath -type d)
INC_DIRS += $(shell find $(MODULE_DIR) -type d)

INC_FLAGS := $(addprefix -I,$(INC_DIRS))


# -------------------------------------------------------------------
# Compiler & linker flags
# -------------------------------------------------------------------
CPU     := bf527
OPTIMISE = -Os
CFLAGS   := -mcpu=$(CPU) $(OPTIMISE) -mfast-fp  -fdata-sections -ffunction-sections -Wall 
CFLAGS   += -DDEBUG -DBLACKFIN

ASM_FLAGS := -x assembler-with-cpp

LDFLAGS := $(LPATH)  -lm -lbfdsp -lbffastfp \
				-T dsp.lds -Xlinker -Map -Xlinker $(BUILD_DIR)/boot.map \
				-Wl,--gc-sections 

LDRFLAGS := --bits 8 --bmode spi_slave --verbose


# -------------------------------------------------------------------
# Source Files
# -------------------------------------------------------------------

SRCS := $(shell find $(SRC_DIR)  \( -name '*.cpp' -or -name '*.c' -or -name '*.S' \) \
		-not -path "$(SRC_DIR)/modules/*")
SRCS += $(shell find $(COMMON_DIR) -name '*.c' -o -name '*.cpp' -o -name '*.S')
SRCS += $(shell find $(LIB_DIR)  \( -name '*.cpp' -or -name '*.c' -or -name '*.S' \) \
		-not -path "$(LIB_DIR)/aleph-dsp/lib/libfixmath/*" \
		-not -path "$(LIB_DIR)/aleph-dsp/test/*")
SRCS += $(shell find $(LIB_DIR)/aleph-dsp/lib/libfixmath/libfixmath  -name '*.cpp' -or \
		-name '*.c' -or -name '*.S')
SRCS += $(shell find $(MODULE_DIR)  -name '*.cpp' -or -name '*.c' -or -name '*.S')

OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

ASMS := $(SRCS:%=$(ASM_DIR)/%.s)



# The -MMD and -MP flags together generate Makefiles for us.
# These files will have .d instead of .o as the output.
CPPFLAGS := $(INC_FLAGS) -MMD -MP -D ARCH_BFIN=1 -D __ADSPBF523__


# Convert ELF to LDR.
$(BUILD_DIR)/$(TARGET_BIN): $(BUILD_DIR)/$(TARGET_EXEC)
	$(LDR) -T $(CPU) -c $(LDRFLAGS) $(BUILD_DIR)/$(TARGET_BIN) $(BUILD_DIR)/$(TARGET_EXEC)

# The final build step.
$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# Build step ASM source.
$(BUILD_DIR)/%.s.o: %.s
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
	$(CC) $(CPPFLAGS) $(CFLAGS) -S $< -o $@

$(ASM_DIR)/%.cpp.s: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -S $< -o $@


.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: asm
asm: $(ASMS)

# Include the .d makefiles. The - at the front suppresses the errors of missing
# Makefiles. Initially, all the .d files will be missing, and we don't want those
# errors to show up.
-include $(DEPS) 
