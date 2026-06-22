#---------------------------------------------------------------------------------
# TrollWare - Nintendo DS Homebrew Makefile
# Requires: devkitARM + libnds (devkitPro toolchain)
#
# Usage:
#   make        -> build TrollWare.nds
#   make clean  -> remove build artefacts
#
# Install devkitPro: https://devkitpro.org/wiki/Getting_Started
#---------------------------------------------------------------------------------

# Guard against running without devkitARM
ifeq ($(strip $(DEVKITARM)),)
    $(error "Please set DEVKITARM in your environment. \
             Install devkitPro and run: export DEVKITARM=/opt/devkitpro/devkitARM")
endif

# Include the devkitARM base rules
include $(DEVKITARM)/ds_rules

#---------------------------------------------------------------------------------
# Project identity
#---------------------------------------------------------------------------------
TARGET   := TrollWare
BUILD    := build
SOURCES  := source
INCLUDES := include
DATA     :=

#---------------------------------------------------------------------------------
# NDS ROM metadata
#---------------------------------------------------------------------------------
GAME_TITLE   := TrollWare
GAME_SUBTITLE1 := Multi-Stage Troll Game
GAME_SUBTITLE2 := devkitARM/libnds
GAME_ICON    :=      # leave blank -> use default devkitPro icon

#---------------------------------------------------------------------------------
# Compiler / linker flags
#---------------------------------------------------------------------------------
ARCH := -mthumb -mthumb-interwork

CFLAGS  := -g -Wall -Wextra -O2 \
            $(ARCH) \
            -fomit-frame-pointer \
            -ffast-math \
            $(INCLUDE)

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS  := $(ARCH)

LDFLAGS  := -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

#---------------------------------------------------------------------------------
# Libraries: nds9 must come before c
#---------------------------------------------------------------------------------
LIBS     := -lnds9 -lc

LIBDIRS  := $(LIBNDS)

#---------------------------------------------------------------------------------
# Source files (auto-discovered)
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(CURDIR)/$(TARGET)
export VPATH   := $(foreach dir,$(SOURCES), $(CURDIR)/$(dir)) \
                  $(foreach dir,$(DATA),    $(CURDIR)/$(dir))

export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(CURDIR)/$(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(CURDIR)/$(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(CURDIR)/$(dir)/*.s)))
BINFILES := $(foreach dir,$(DATA),   $(notdir $(wildcard $(CURDIR)/$(dir)/*.*)))

export OFILES_BIN  := $(addsuffix .o,$(BINFILES))
export OFILES_SRC  := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES      := $(OFILES_BIN) $(OFILES_SRC)

export HFILES      := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE     := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                      $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                      -I$(CURDIR)/$(BUILD)

export LIBPATHS    := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean

$(BUILD):
	@echo "Building $(TARGET).nds ..."
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo "Cleaning build artefacts..."
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds $(TARGET).map

else  # we ARE in the build directory

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).nds : $(OUTPUT).elf
$(OUTPUT).elf : $(OFILES)

-include $(DEPENDS)

endif
