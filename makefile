CXX = g++ -DIMGUI_DEFINE_MATH_OPERATORS
GLFW_MACRO = _GLFW_WAYLAND

OBJ_RAW = glad.o imgui_demo.o imgui_draw.o imgui_impl_glfw.o imgui_impl_opengl3.o imgui_tables.o imgui_widgets.o imgui.o libglfw3.a
OBJ_RAW_ = file_dialogue.o
OBJ = $(addprefix ./.o/vendor/, $(OBJ_RAW)) $(addprefix ./.o/, $(OBJ_RAW_))

INCLUDE_RAW = /glad/include /glfw/include /glm / /imgui
INCLUDE = $(addprefix -I ./submodule,$(INCLUDE_RAW)) -I ./include

run: build
	./.bin/main

buildo: $(patsubst ./src/%.cpp, ./.o/%.o, $(wildcard ./src/*.cpp))
./.o/%.o: ./src/%.cpp
	-@echo -e "\033[0;32mBuilding $@\033[0;36m"
	$(CXX) $< -o $@ -c $(INCLUDE)
	@echo -e "\033[0;32mBuilt $@\033[0m"

build: buildo ./main.cpp ./include/*
	$(CXX) $(INCLUDE) ./main.cpp $(OBJ) -o ./.bin/main


build_deps:
	mkdir ./.o/vendor
# Glad Files
	$(CXX) ./submodule/glad/src/glad.c -c -o ./.o/vendor/glad.o -I ./submodule/glad/include
# GLFW files
	cd submodule && cmake -S ./glfw -B ./glfw_build && cmake --build ./glfw_build
	cp ./submodule/glfw_build/src/libglfw3.a ./.o/vendor/libglfw3.a
# Imgui Files
	$(CXX) ./submodule/imgui/imgui_demo.cpp -o ./.o/vendor/imgui_demo.o -I ./submodule/imgui -c
	$(CXX) ./submodule/imgui/imgui_draw.cpp -o ./.o/vendor/imgui_draw.o -I ./submodule/imgui -c
	$(CXX) ./submodule/imgui/imgui_tables.cpp -o ./.o/vendor/imgui_tables.o -I ./submodule/imgui -c
	$(CXX) ./submodule/imgui/imgui_widgets.cpp -o ./.o/vendor/imgui_widgets.o -I ./submodule/imgui -c
	$(CXX) ./submodule/imgui/imgui.cpp -o ./.o/vendor/imgui.o -I ./submodule/imgui -c
# ImGui Backend Files
	$(CXX) ./submodule/imgui/backends/imgui_impl_glfw.cpp -o ./.o/vendor/imgui_impl_glfw.o -I ./submodule/imgui -c
	$(CXX) ./submodule/imgui/backends/imgui_impl_opengl3.cpp -o ./.o/vendor/imgui_impl_opengl3.o -I ./submodule/imgui -c