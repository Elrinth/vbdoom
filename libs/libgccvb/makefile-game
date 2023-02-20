#Makefile taken from Wikipedia.org
#

# Specify the main target
BUILD_DIR = build

# Specify the main target
TARGET = $(BUILD_DIR)/output

# Default build type
#TYPE = debug
TYPE = release
#TYPE = preprocessor

# Libgccvb's home
LIBGCCVB_PATH = $(VBDE)/libs/libgccvb

# Which directories contain source files
DIRS := $(shell find . $(LIBGCCVB_PATH) -path ./build -prune -o -type d -print)
		
# Which libraries are linked
LIBS =
ROMHEADER=lib/vb.hdr

# Dynamic libraries
DLIBS =


GAME_ESSENTIALS = -include $(LIBGCCVB_PATH)/libgccvb.h 

# The next blocks change some variables depending on the build type
ifeq ($(TYPE),debug)
LDPARAM = -T$(LIBGCCVB_PATH)/compiler/vb.ld -lm
CCPARAM = -fno-builtin -ffreestanding -nodefaultlibs -mv810 -O -Wall $(GAME_ESSENTIALS) 
MACROS = __DEBUG
endif

ifeq ($(TYPE), release)
LDPARAM = -T$(LIBGCCVB_PATH)/compiler/vb.ld -lm
CCPARAM = -nodefaultlibs -mv810 -finline-functions -Wall -O3 -Winline $(GAME_ESSENTIALS) -I$(VBDE)/libs/libgccvb/
MACROS = NDEBUG
endif

ifeq ($(TYPE),preprocessor)
LDPARAM = -T$(LIBGCCVB_PATH)/compiler/vb.ld -lm
CCPARAM = -nodefaultlibs -mv810 -Wall -Winline $(GAME_ESSENTIALS) -E
MACROS = __DEBUG
endif

# Add directories to the include and library paths
INCPATH_GAME := $(shell find * -type d -print)

LIBPATH =

# Which files to add to backups, apart from the source code
EXTRA_FILES = makefile

# The compiler
GCC = v810-gcc
AS = v810-as
OBJCOPY = v810-objcopy
OBJDUMP = v810-objdump

# Where to store object and dependancy files.
STORE = $(BUILD_DIR)/$(TYPE)

# Makes a list of the source (.cpp) files.
SOURCE := $(foreach DIR,$(DIRS),$(wildcard $(DIR)/*.c))

# Makes a list of the source (.s) files.
ASM := $(foreach DIR,$(DIRS),$(wildcard $(DIR)/*.s))

# List of header files.
HEADERS := $(foreach DIR,$(DIRS),$(wildcard $(DIR)/*.h))

# Makes a list of the object files that will have to be created.
C_OBJECTS := $(addprefix $(STORE)/, $(SOURCE:.c=.o))

# Makes a list of the object files that will have to be created.
ASM_OBJECTS := $(addprefix $(STORE)/, $(ASM:.s=.o))

# Same for the .d (dependancy) files.
DFILES := $(addprefix $(STORE)/,$(SOURCE:.c=.d))

# Specify phony rules. These are rules that are not real files.
.PHONY: clean backup dirs

# Main target. The @ in front of a command prevents make from displaying
# it to the standard output.

all: $(TARGET).vb

$(TARGET).vb: main.elf
	@echo Creating $@
	@$(OBJCOPY) -O binary main.elf $@
	@echo $(TARGET).vb done
#	@echo Generating assembler code
#	@$(OBJDUMP) -t main.elf > sections.txt
#	@$(OBJDUMP) -S main.elf > machine.asm
	

main.elf: dirs $(C_OBJECTS) $(ASM_OBJECTS)
		@echo Linking $(TARGET).
		@$(GCC) -o $@ -nostartfiles $(C_OBJECTS) $(ASM_OBJECTS) $(LDPARAM) \
			$(foreach LIBRARY, $(LIBS),-l$(LIBRARY)) $(foreach LIB,$(LIBPATH),-L$(LIB))

# Rule for creating object file and .d file, the sed magic is to add
# the object path at the start of the file because the files gcc
# outputs assume it will be in the same dir as the source file.
$(STORE)/%.o: %.c
		@echo Creating object file for $*...
		@$(GCC) -Wp,-MD,$(STORE)/$*.dd $(CCPARAM) $(foreach INC, $(INCPATH_GAME),-I$(INC))\
                $(foreach MACRO,$(MACROS),-D$(MACRO)) -c $< -o $@
		@sed -e '1s/^\(.*\)$$/$(subst /,\/,$(dir $@))\1/' $(STORE)/$*.dd > $(STORE)/$*.d
		@rm -f $(STORE)/$*.dd
		
$(STORE)/%.o: %.s
	@echo Creating object file for $*
	@$(AS) -o $@ $<
		
# Empty rule to prevent problems when a header is deleted.
%.h: ;

# Cleans up the objects, .d files and executables.
clean:
		@echo Making clean.
		@-rm -f $(foreach DIR,$(DIRS),$(STORE)/$(DIR)/*.d $(STORE)/$(DIR)/*.o)
		@-rm -Rf $(STORE)
		@-rm -f $(ENGINE)

# Backup the source files.
backup:
		@-if [ ! -e .backup ]; then mkdir .backup; fi;
		@zip .backup/backup_`date +%d-%m-%y_%H.%M`.zip $(SOURCE) $(HEADERS) $(EXTRA_FILES)

# Create necessary directories
dirs:
		@-if [ ! -e $(STORE) ]; then mkdir -p $(STORE); fi;
		@-$(foreach DIR,$(DIRS), if [ ! -e $(STORE)/$(DIR) ]; \
         then mkdir -p $(STORE)/$(DIR); fi; )

# Includes the .d files so it knows the exact dependencies for every
# source.
-include $(DFILES)