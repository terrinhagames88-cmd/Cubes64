BUILD_DIR = build
# Este projeto usa OpenGL, que e uma API "preview" do libdragon
LIBDRAGON_PREVIEW = 2
include $(N64_INST)/include/n64.mk

src = main.c
assets_png = $(wildcard t_*.png)
assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

all: cubos64.z64

filesystem/%.sprite: %.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) -f RGBA16 -o "$(dir $@)" "$<"

$(BUILD_DIR)/cubos64.dfs: $(assets_conv)
$(BUILD_DIR)/cubos64.elf: $(src:%.c=$(BUILD_DIR)/%.o)

cubos64.z64: N64_ROM_TITLE = "Cubos 64"
cubos64.z64: $(BUILD_DIR)/cubos64.dfs

clean:
	rm -rf $(BUILD_DIR) filesystem cubos64.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
