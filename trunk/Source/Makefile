#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
VERSION		:=	53
RELEASE		:=	devel
TARGET		:=	cfg$(VERSION)
BUILD		:=	build
SOURCES		:=	source source/libpng/pngu source/libwbfs source/unzip
DATA		:=	data
INCLUDES	:=	
GETTEXT		:=	tools/gettext-bin
UPLOAD		:=  tools/googlecode_upload.py
GAUTH		?=  -u username -w passwd

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------

FAT_BUILD	:=	0
ifeq ($(FAT_BUILD),1)
	BUILD	:=	build_fat
	TARGET		:=	cfg$(VERSION)-fat
endif
BUILD_FLAGS = -DFAT_BUILD=$(FAT_BUILD) -DVERSION=$(VERSION)
#"-g" tells the compiler to include support for the debugger
#"-Wall" tells it to warn us about all suspicious-looking code
DEBUG_OPT	= -Os
#DEBUG_OPT	= -g
CFLAGS	= $(DEBUG_OPT) -Wall $(MACHDEP) $(INCLUDE) $(BUILD_FLAGS)
CXXFLAGS	=	$(CFLAGS)

# start address:
# Original:   0x80a00100
# NeoGammaR4: 0x80dfff00
# cfg 33-36:  0x80c00000
# cfg 37-49:  0x80b00000
# cfg 50-..:  0x80a80000
LDFLAGS	=	$(MACHDEP) -Wl,-Map,$(notdir $@).map,--section-start,.init=0x80a80000

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
#LIBS	:=	-lmetaphrasis -lfreetype -lasnd -lpng -lz -lfat -lwiiuse -lbte -lmad -lmodplay -logc -lm -ltremor
LIBS	:=	-lfat -lntfs -lpng -lwiiuse -lbte -lmad -lmodplay -lasnd -logc -lm -lz

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
#LIBDIRS	:=	$(DEVKITPPC)/lib $(CURDIR) $(CURDIR)/lib/png $(CURDIR)/lib/freetype $(CURDIR)/lib/libfat
LIBDIRS	:=	$(DEVKITPPC)/lib $(CURDIR) $(CURDIR)/lib/png $(CURDIR)/lib/libfat $(CURDIR)/lib/libntfs

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export PATH 	:=	$(CURDIR)/$(GETTEXT):$(PATH)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
					$(sFILES:.s=.o) $(SFILES:.S=.o)

#---------------------------------------------------------------------------------
# build a list of include paths
#---------------------------------------------------------------------------------
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD) \
					-I$(LIBOGC_INC) -I$(LIBOGC_INC)/ogc -I$(LIBOGC_INC)/ogc/machine

#---------------------------------------------------------------------------------
# build a list of library paths
#---------------------------------------------------------------------------------
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
					-L$(LIBOGC_LIB)

export OUTPUT	:=	$(CURDIR)/$(TARGET)
.PHONY: $(BUILD) lang all clean

#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@echo
	@echo Building $(TARGET) $(BUILD_FLAGS)
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

fat:
	@make --no-print-directory FAT_BUILD=1

#---------------------------------------------------------------------------------
lang:
	@[ -d Languages ] || mkdir -p Languages
	@echo
	@echo "Building Language File ($@$(VERSION).pot)"
	@xgettext --sort-output --no-wrap --no-location -k -kgt -o Languages/$@$(VERSION).pot source/*.c

#---------------------------------------------------------------------------------
all:
	@echo Make All
	@make --no-print-directory build
	@make --no-print-directory FAT_BUILD=1
	@make --no-print-directory lang

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol
	@rm -fr $(BUILD)_fat $(OUTPUT)-fat.elf $(OUTPUT)-fat.dol

#---------------------------------------------------------------------------------
run: 
	wiiload $(OUTPUT).dol debug=0 simple=0 home=exit gui=coverflow3d gui_style=grid gui_antialias=4 gui_compress_covers=1 music=sd://mp3
#	wiiload $(OUTPUT).dol simple=0 device=usb cover_style=3d widescreen=1 covers_size=100,100 debug=1 music=sd://mp3 gui_font=Vera_Mono_12_Bold.png hide_game=RB7E admin_lock=1
# widescreen = [auto], 0, 1
# home = [reboot], exit, screenshot
# cover_style = [standard], 3d, disc

DATE=$(shell date +%F)
SIZE=$(shell stat -c %s cfg$(VERSION).dol 2>/dev/null)
SIZE_FAT=$(shell stat -c %s cfg$(VERSION)-fat.dol 2>/dev/null)

define ANNOUNCE_SH
CHANGES=`sed -n '/^cfg v'$(VERSION)' /,/^cfg/{/^cfg/!p}' README-CFG.txt`
echo "===== updates.txt: ====="
cat << END

release = $(VERSION) ($(RELEASE))
size = $(SIZE)
date = $(DATE)
url = http://cfg-loader.googlecode.com/files/cfg$(VERSION).dol
$$CHANGES

release = $(VERSION)-fat ($(RELEASE))
size = $(SIZE_FAT)
date = $(DATE)
url = http://cfg-loader.googlecode.com/files/cfg$(VERSION)-fat.dol
Same as $(VERSION) but with different
default options:
  ios=222-mload
  partition=FAT1
  hide_hddinfo=1

END
echo "===== forum ====="
cat << END

[b]cfg v$(VERSION) ($(RELEASE))[/b]
[url="http://cfg-loader.googlecode.com/files/cfg$(VERSION).dol"]cfg$(VERSION).dol[/url]
[url="http://cfg-loader.googlecode.com/files/cfg$(VERSION)-fat.dol"]cfg$(VERSION)-fat.dol[/url]
[url="http://cfg-loader.googlecode.com/files/lang$(VERSION).zip"]lang$(VERSION).zip[/url]
(or online update)

[pre]Changes:
cfg v$(VERSION) ($(RELEASE))
$$CHANGES
[/pre]

END
endef
export ANNOUNCE_SH

announce:
	@echo "$$ANNOUNCE_SH" | sh

DOLS := cfg$(VERSION)-fat.dol cfg$(VERSION).dol
ELFS := cfg$(VERSION)-fat.elf cfg$(VERSION).elf
BINS := $(ELFS) $(DOLS)

upload:
	for F in $(BINS); do echo uploading $$F ; $(UPLOAD) $(GAUTH) -p cfg-loader -s $$F $$F ; done

upload_lang:
	echo uploading Languages/lang$(VERSION).zip
	$(UPLOAD) $(GAUTH) -p cfg-loader -s lang$(VERSION).zip Languages/lang$(VERSION).zip

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

#---------------------------------------------------------------------------------
# This rule links in binary data with the .jpg extension
#---------------------------------------------------------------------------------
%.jpg.o	:	%.jpg
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	$(bin2o)

%.ttf.o : %.ttf
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
