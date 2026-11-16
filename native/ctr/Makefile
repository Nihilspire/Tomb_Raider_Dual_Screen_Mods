# Tomb Raider Dual 3DS (alpha): pcsx_rearmed at c881679 (ari64 dynarec + Unai software renderer, the core Tomb Raider Dual uses on
# Android) with a libctru frontend: the game on the top screen, Tomb Raider Link in C on the bottom one.
#   make -C native/ctr                 -> $(OUT)/Tomb Raider Dual 3DS.3dsx
# Needs devkitPro's 3ds-dev and node. The core's source is Xenogears Dual's (XGDUAL), as for the Android core (android/dual/build_jni.sh);
# the source list and flags are Xenogears Dual 3DS's (its native/ctr/Makefile), which are those of "platform ctr" in the core's
# Makefile.libretro. The core's own files are not changed.
DEVKITPRO	?= /opt/devkitpro
DEVKITARM	?= $(DEVKITPRO)/devkitARM
# relative: the project's folder name has spaces, which make can't take in a target or prerequisite
ROOT		:= ../..
XGDUAL		?= $(abspath ../../../Xenogears/xenogears-dual)
S		:= $(XGDUAL)/third_party/pcsx_rearmed
OUT		?= $(HOME)/tr3ds/ctr
OBJDIR		?= $(OUT)/obj
VERSION		:= 0.2-alpha
# test builds (test/build_test.sh): EXTRA="-DCTR_TEST -I<dir with test_script.h>", OUT elsewhere
EXTRA		?=

PREFIX		:= $(DEVKITARM)/bin/arm-none-eabi-
CC		:= $(PREFIX)gcc
CXX		:= $(PREFIX)g++
ARCH		:= -march=armv6k -mtune=mpcore -mfloat-abi=hard -marm -mfpu=vfp -mtp=soft

C		:= libpcsxcore
LC		:= deps/libretro-common
LCHDR		:= deps/libchdr
LZ		:= $(LCHDR)/deps/lzma-26.02
ZS		:= $(LCHDR)/deps/zstd-1.5.7
MZ		:= $(LCHDR)/deps/miniz-3.1.2

CORE_C		:= $(addprefix $(C)/, cdriso cdrom cdrom-async cheat database decode_xa mdec misc pad plugins ppf psxbios \
		   psxcommon psxcounters psxdma psxevents psxhw psxinterpreter psxmem r3000a sio spu gpu gte gte_nf gte_divider \
		   new_dynarec/new_dynarec new_dynarec/pcsxmem new_dynarec/emu_if) \
		   $(addprefix plugins/dfsound/, dma freeze registers spu out nullsnd) \
		   $(addprefix plugins/gpulib/, gpu prim vout_pl gpu_async) \
		   plugins/cdrcimg/cdrcimg \
		   $(addprefix frontend/, main plugin cspace libretro pcsxr-threads) \
		   $(addprefix $(LCHDR)/src/libchdr_, bitstream cdrom chd codec_cdfl codec_cdlz codec_cdzl codec_cdzs codec_flac \
		   codec_huff codec_lzma codec_zlib codec_zstd flac huffman) \
		   $(LZ)/src/LzmaDec $(ZS)/zstddeclib $(MZ)/miniz \
		   $(addprefix $(LC)/, compat/compat_posix_string compat/fopen_utf8 compat/compat_strl encodings/encoding_utf \
		   file/file_path file/file_path_io file/retro_dirent streams/file_stream streams/file_stream_transforms \
		   string/stdstring string/rstrtod time/rtime vfs/vfs_implementation features/features_cpu)
CORE_CPP	:= plugins/gpu_unai/gpulib_if plugins/gpu_unai/old/if
CORE_S		:= $(C)/gte_arm $(C)/gte_nf_arm $(C)/new_dynarec/linkage_arm plugins/dfsound/arm_utils frontend/cspace_arm \
		   plugins/gpu_unai/gpu_arm deps/arm-mem/memcpymove-v6l deps/arm-mem/memset-v6l

DEFS		:= -DARM11 -D_3DS -D__3DS__ -DGPU_UNAI_USE_FLOATMATH -DGPU_UNAI_USE_FLOAT_DIV_MULTINV -DHAVE_UNISTD_H \
		   -DZ7_DECL_Int32_AS_long -DUSE_CTRULIB_2 -DNO_DYLIB -DNDEBUG -DP_HAVE_MMAP=0 -DP_HAVE_POSIX_MEMALIGN=0 \
		   -DDISABLE_MEM_LUTS=0 -DGPU_UNAI -DUSE_GPULIB=1 -DBUILTIN_GPU=unai -DHAVE_LIBRETRO -DNO_FRONTEND \
		   -DHAVE_CHD -DUSE_MINIZ -DMINIZ_NO_STDIO -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_ARCHIVE_APIS -DUSE_LIBRETRO_VFS \
		   -DLIGHTREC_CUSTOM_MAP=0 -DNDRC_THREAD -DUSE_ASYNC_CDROM -DUSE_ASYNC_GPU -DUSE_ASYNC_SPU -DHAVE_RTHREADS \
		   -DHAVE_bgr555_to_rgb565 -DGIT_VERSION=\"c881679\"
INCS		:= -I$(XGDUAL)/core/inc -I$(S)/frontend/3ds -I$(S)/include -I$(S)/deps/crypto -I$(S)/$(LCHDR)/include -I$(S)/$(LZ)/include \
		   -I$(S)/$(ZS) -I$(S)/$(MZ) -I$(S)/$(LC)/include -I$(DEVKITPRO)/libctru/include
COREFLAGS	:= -O3 -ffast-math -fno-strict-aliasing -ffunction-sections -fdata-sections -fomit-frame-pointer \
		   -mword-relocations $(ARCH) $(DEFS) $(INCS) -w -ffile-prefix-map=$(S)=pcsx_rearmed
FEFLAGS		:= -O2 -std=gnu11 -ffunction-sections -fdata-sections $(ARCH) -D__3DS__ -DNDEBUG -Wall -Wno-unused-function -Wno-misleading-indentation \
		   -I$(S)/include -I$(S)/$(LC)/include -I$(DEVKITPRO)/libctru/include -I$(OBJDIR) $(EXTRA)
LDFLAGS		:= -specs=3dsx.specs $(ARCH) -Wl,--gc-sections -L$(DEVKITPRO)/libctru/lib

OBJS		:= $(addprefix $(OBJDIR)/core/,$(addsuffix .o,$(CORE_C) $(CORE_CPP) $(CORE_S))) \
		   $(OBJDIR)/core/utils.o $(OBJDIR)/main.o $(OBJDIR)/linkscr.o

all: $(OUT)/Tomb\ Raider\ Dual\ 3DS.3dsx

$(OBJDIR)/core/%.o: $(S)/%.c
	@mkdir -p $(dir $@)
	@$(CC) -std=gnu11 $(COREFLAGS) -c $< -o $@ || (echo FAIL $<; false)
# new_dynarec at -O2, "less bloat" as in the core's Makefile
$(OBJDIR)/core/$(C)/new_dynarec/new_dynarec.o: $(S)/$(C)/new_dynarec/new_dynarec.c
	@mkdir -p $(dir $@)
	@$(CC) -std=gnu11 $(COREFLAGS) -O2 -c $< -o $@ || (echo FAIL $<; false)
$(OBJDIR)/core/plugins/gpu_unai/gpulib_if.o: $(S)/plugins/gpu_unai/gpulib_if.cpp
	@mkdir -p $(dir $@)
	@$(CXX) -std=gnu++11 -fno-rtti -fno-exceptions $(COREFLAGS) -DREARMED -c $< -o $@ || (echo FAIL $<; false)
$(OBJDIR)/core/%.o: $(S)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) -std=gnu++11 -fno-rtti -fno-exceptions $(COREFLAGS) -c $< -o $@ || (echo FAIL $<; false)
$(OBJDIR)/core/%.o: $(S)/%.S
	@mkdir -p $(dir $@)
	@$(CC) $(COREFLAGS) -c $< -o $@ || (echo FAIL $<; false)
# the core's cache and backdoor calls, renamed so main.c can choose between them and Azahar's (see "the recompiler's cache")
$(OBJDIR)/core/utils.o: $(S)/frontend/3ds/utils.S
	@mkdir -p $(dir $@)
	@$(CC) $(COREFLAGS) -Dctr_clear_cache=hw_clear_cache -Dctr_clear_cache_range=hw_clear_cache_range \
	  -Dctr_invalidate_icache=hw_invalidate_icache -c $< -o $@

# the story steps, from the same hints.js the phone and Windows apps use
$(OBJDIR)/hintdata.h: tools/mk_hints.js $(ROOT)/app/www/hints.js
	@mkdir -p $(dir $@)
	node tools/mk_hints.js $(ROOT)/app/www/hints.js $@
$(OBJDIR)/main.o: main.c linkscr.h
	$(CC) $(FEFLAGS) -c $< -o $@
$(OBJDIR)/linkscr.o: linkscr.c linkscr.h $(OBJDIR)/hintdata.h
	$(CC) $(FEFLAGS) -c $< -o $@

# the map pictures, names and letters, made from the player's own disc: game data, so a file of its own for the SD card, not part
# of `all`. DISC=<Track 01 .bin, .cue or .chd>
pak: $(OUT)/tomb-raider-link.pak
$(OUT)/tomb-raider-link.pak: tools/mk_pak.js $(ROOT)/app/www/trlevel.js $(ROOT)/app/www/trmap.js $(ROOT)/app/www/trbuild.js
	node tools/mk_pak.js "$(DISC)" $@

$(OUT)/tombraider3ds.elf: $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -lctru -lm -o $@

$(OUT)/tombraider3ds.smdh: tools/icon.png
	$(DEVKITPRO)/tools/bin/smdhtool --create "Tomb Raider Dual 3DS" "$(VERSION): your disc + a Link screen" "Jack" $< $@

$(OUT)/Tomb\ Raider\ Dual\ 3DS.3dsx: $(OUT)/tombraider3ds.elf $(OUT)/tombraider3ds.smdh
	$(DEVKITPRO)/tools/bin/3dsxtool $< "$@" --smdh=$(OUT)/tombraider3ds.smdh
	@ls -la "$@"

clean:
	rm -rf $(OBJDIR) $(OUT)/*.elf $(OUT)/*.3dsx $(OUT)/*.smdh
