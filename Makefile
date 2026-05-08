# MARK: - style control

V ?= 0

# verbose output
ifeq ($(V),0)
	Q    = @
	ECHO = @echo
else
	Q    =
	ECHO = @true
endif

# color table
ifneq ($(shell tput colors 2>/dev/null),)
	ESC        := \033[
	NC         := $(ESC)0m
	COLOR_CC   := $(ESC)0;34m
	COLOR_AS   := $(ESC)0;33m
	COLOR_LD   := $(ESC)0;32m
	COLOR_MKFS := $(ESC)0;35m
	COLOR_RUN  := $(ESC)0;36m
	COLOR_OK   := $(ESC)0;32m
	COLOR_ERR  := $(ESC)0;31m
else
	COLOR_CC = COLOR_AS = COLOR_LD = \
	COLOR_MKFS = COLOR_RUN = COLOR_OK = \
	COLOR_ERR = NC =
endif

# MARK: - toolchain

PREFIX ?= $(shell ./scripts/find_riscv_toolchain.sh 2>/dev/null)

ifeq ($(PREFIX),ERROR)
	$(error Could not find RISC-V toolchain in PATH!)
endif

PREFIX := $(strip $(PREFIX))

# apply prefix
CC      = $(PREFIX)gcc
AS      = $(PREFIX)as
LD      = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump
GDB     = $(PREFIX)gdb

# MARK: - variables

K=kernel
U=user
I=include
BUILD_DIR=build
LINKER_DIR=linker

include config.mk

KERNEL_LD=$(LINKER_DIR)/kernel.ld
USER_LD=$(LINKER_DIR)/user.ld

# qemu
QEMU ?= qemu-system-riscv64
MIN_QEMU_VERSION ?= 7.2

# compiler flags
CFLAGS = -Wall -Werror -Wno-unknown-attributes -O -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -march=rv64gc -mabi=lp64
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding
CFLAGS += -fno-common -nostdlib
CFLAGS += -fno-builtin-strncpy -fno-builtin-strncmp -fno-builtin-strlen -fno-builtin-memset
CFLAGS += -fno-builtin-memmove -fno-builtin-memcmp -fno-builtin-log -fno-builtin-bzero
CFLAGS += -fno-builtin-strchr -fno-builtin-exit -fno-builtin-malloc -fno-builtin-putc
CFLAGS += -fno-builtin-free
CFLAGS += -fno-builtin-memcpy -Wno-main
CFLAGS += -fno-builtin-printf -fno-builtin-fprintf -fno-builtin-vprintf
CFLAGS += -fdiagnostics-color=always

KERNEL_CPPFLAGS = -I$(K)/include -I$(K)/riscv -I$(I)
USER_CPPFLAGS = -I$(U)/include -I$(I)
MKFS_CPPFLAGS = -iquote $(I)

# linker flags
LDFLAGS = -z max-page-size=4096
LDFLAGS += -melf64lriscv

# disable stack protector
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

# disable PIE
CFLAGS += $(shell $(CC) -dumpspecs 2>/dev/null | grep -q 'no-pie' && echo '-fno-pie -no-pie')

# MARK: - helper targets

.PHONY: all
all: kernel fs.img

.PHONY: toolchain-info
toolchain-info:
	$(ECHO) "$(COLOR_OK)[Toolchain Prefix] $(COLOR_CC)$(PREFIX)$(NC)"

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# MARK: - kernel targets

KERNEL = $(BUILD_DIR)/kernel.elf

KERNEL_C_SRCS = $(wildcard $(K)/*.c)
KERNEL_S_SRCS = $(wildcard $(K)/*.S) $(wildcard $(K)/riscv/*.S)

KERNEL_C_OBJS = $(patsubst $(K)/%.c,$(BUILD_DIR)/$(K)/%.o,$(KERNEL_C_SRCS))
KERNEL_S_OBJS = $(patsubst $(K)/%.S,$(BUILD_DIR)/$(K)/%.o,$(KERNEL_S_SRCS))
KERNEL_OBJS = $(KERNEL_C_OBJS) $(KERNEL_S_OBJS)

$(BUILD_DIR)/$(K)/%.o: $(K)/%.c
	@mkdir -p $(@D)
	$(ECHO) "$(COLOR_CC)  CC  $(NC)$@"
	$(Q)$(CC) $(CFLAGS) $(KERNEL_CPPFLAGS) -c -o $@ $<

$(BUILD_DIR)/$(K)/%.o: $(K)/%.S
	@mkdir -p $(@D)
	$(ECHO) "$(COLOR_AS)  AS  $(NC)$@"
	$(Q)$(CC) $(CFLAGS) $(KERNEL_CPPFLAGS) -c -o $@ $<

$(KERNEL): $(KERNEL_OBJS) $(KERNEL_LD)
	$(ECHO) "$(COLOR_LD)  LD  $(NC)$@"
	$(Q)$(LD) $(LDFLAGS) -T $(KERNEL_LD) -o $@ $(KERNEL_OBJS)
	$(Q)$(OBJDUMP) -S $@ > $(BUILD_DIR)/kernel.asm
	$(Q)$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(BUILD_DIR)/kernel.sym

.PHONY: kernel
kernel: $(KERNEL)

# MARK: - user targets

USER_LIB_SRCS = $(addprefix $(U)/,ulib.c printf.c umalloc.c)
USER_LIB_OBJS = $(patsubst $(U)/%.c,$(BUILD_DIR)/$(U)/%.o,$(USER_LIB_SRCS))
USYS_S = $(BUILD_DIR)/$(U)/usys.S
USYS_OBJ = $(BUILD_DIR)/$(U)/usys.o
ULIB = $(USER_LIB_OBJS) $(USYS_OBJ)

USER_PROG_SRCS = $(filter-out $(USER_LIB_SRCS),$(wildcard $(U)/*.c))
UPROGS = $(patsubst $(U)/%.c,$(BUILD_DIR)/$(U)/_%,$(USER_PROG_SRCS))

$(BUILD_DIR)/$(U)/%.o: $(U)/%.c
	@mkdir -p $(@D)
	$(ECHO) "$(COLOR_CC)  CC  $(NC)$@"
	$(Q)$(CC) $(CFLAGS) $(USER_CPPFLAGS) -c -o $@ $<

$(USYS_S): $(U)/usys.pl
	@mkdir -p $(@D)
	$(ECHO) "$(COLOR_AS)  GEN $(NC)$@"
	$(Q)perl $< > $@

$(USYS_OBJ): $(USYS_S)
	@mkdir -p $(@D)
	$(ECHO) "$(COLOR_AS)  AS  $(NC)$@"
	$(Q)$(CC) $(CFLAGS) $(USER_CPPFLAGS) -c -o $@ $<

$(BUILD_DIR)/$(U)/_%: $(BUILD_DIR)/$(U)/%.o $(ULIB) $(USER_LD)
	$(ECHO) "$(COLOR_LD)  LD  $(NC)$@"
	$(Q)$(LD) $(LDFLAGS) -T $(USER_LD) -o $@ $< $(ULIB)
	$(Q)$(OBJDUMP) -S $@ > $(BUILD_DIR)/$(U)/$*.asm
	$(Q)$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(BUILD_DIR)/$(U)/$*.sym

.PHONY: user
user: $(UPROGS)

.PRECIOUS: $(BUILD_DIR)/$(K)/%.o $(BUILD_DIR)/$(U)/%.o $(USYS_S)

# MARK: - filesystem targets

MKFS = $(BUILD_DIR)/mkfs/mkfs
FS_IMG = $(BUILD_DIR)/fs.img

$(MKFS): mkfs/mkfs.c $(wildcard $(I)/*.h)
	@mkdir -p $(@D)
	$(ECHO) "$(COLOR_CC)  CC  $(NC)$@"
	$(Q)gcc -Wno-unknown-attributes $(MKFS_CPPFLAGS) -o $@ $<

$(FS_IMG): $(MKFS) README.md $(UPROGS)
	$(ECHO) "$(COLOR_MKFS)MKFS  $(NC)$@"
	$(Q)$(MKFS) $@ README.md $(UPROGS)

.PHONY: fs.img
fs.img: $(FS_IMG)

# MARK: - qemu targets

QEMUOPTS = -machine virt -bios none -kernel $(KERNEL) -m 128M -smp $(CPUS) -nographic
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=$(FS_IMG),if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

QEMU_VERSION = $(shell $(QEMU) --version 2>/dev/null | sed -nE '1s/.*QEMU emulator version ([0-9]+(\.[0-9]+)?).*/\1/p')

.PHONY: check-qemu-version
check-qemu-version:
	$(Q)if [ -z "$(QEMU_VERSION)" ]; then \
		echo "ERROR: Could not determine qemu version"; \
		exit 1; \
	fi
	$(Q)if ! awk -v have="$(QEMU_VERSION)" -v need="$(MIN_QEMU_VERSION)" 'BEGIN { split(have, h, "."); split(need, n, "."); for (i = 1; i <= 2; i++) { if (h[i] + 0 > n[i] + 0) exit 0; if (h[i] + 0 < n[i] + 0) exit 1; } exit 0 }'; then \
		echo "ERROR: Need qemu version >= $(MIN_QEMU_VERSION)"; \
		exit 1; \
	fi

.PHONY: qemu
qemu: check-qemu-version $(KERNEL) $(FS_IMG)
	$(ECHO) "$(COLOR_RUN)QEMU  $(NC)$(KERNEL)"
	$(Q)$(QEMU) $(QEMUOPTS)

-include $(KERNEL_OBJS:.o=.d)
-include $(USER_LIB_OBJS:.o=.d) $(USYS_OBJ:.o=.d)
-include $(patsubst $(U)/%.c,$(BUILD_DIR)/$(U)/%.d,$(USER_PROG_SRCS))
