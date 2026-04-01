# QL Studio — ImGui + SDL2
#
# Build: make
# Run: ./qlstudio [project.json]

CC = cc
CXX = c++

SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS = $(shell sdl2-config --libs)

CXXFLAGS = -std=c++17 -O2 -Wall -I thirdparty/imgui $(SDL_CFLAGS)
CFLAGS = -O2 -Wall $(SDL_CFLAGS)

LDFLAGS = $(SDL_LIBS) -framework OpenGL

TARGET = qlstudio
BUILD = build

# ImGui sources
IMGUI_SRCS = \
	thirdparty/imgui/imgui.cpp \
	thirdparty/imgui/imgui_draw.cpp \
	thirdparty/imgui/imgui_tables.cpp \
	thirdparty/imgui/imgui_widgets.cpp \
	thirdparty/imgui/imgui_demo.cpp \
	thirdparty/imgui/imgui_impl_sdl2.cpp \
	thirdparty/imgui/imgui_impl_opengl3.cpp

# App sources
APP_SRCS = \
	src/main.cpp \
	src/app.cpp

# C sources (none currently)
C_SRCS =

IMGUI_OBJS = $(patsubst %.cpp,$(BUILD)/%.o,$(IMGUI_SRCS))
APP_OBJS = $(patsubst %.cpp,$(BUILD)/%.o,$(APP_SRCS))
C_OBJS = $(patsubst %.c,$(BUILD)/%.o,$(C_SRCS))

ALL_OBJS = $(IMGUI_OBJS) $(APP_OBJS) $(C_OBJS)

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(ALL_OBJS) $(LDFLAGS)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET)

.PHONY: all clean
