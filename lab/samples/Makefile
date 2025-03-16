# Special tools.
LDFLAGS_X11			= `pkg-config --cflags --libs x11` -D__X11__
LDFLAGS_SDL			= -lSDL2 -D__SDL__

# Rules.
.PHONY: x11
x11: 
	$(CXX) -o $(BUILD_DIR)/minimal 1_minimal.cpp $(CXXFLAGS) $(LDFLAGS_X11)  
	$(CXX) -o $(BUILD_DIR)/raster 2_raster.cpp resources/tut_raster.cpp $(CXXFLAGS) $(LDFLAGS_X11) 
	$(CXX) -o $(BUILD_DIR)/sound 3_sound.cpp resources/power_on_wav.cpp $(CXXFLAGS) $(LDFLAGS_X11) 


.PHONY: sdl
sdl: 
	$(CXX) -o $(BUILD_DIR)/minimal 1_minimal.cpp $(CXXFLAGS) $(LDFLAGS_SDL) 
	$(CXX) -o $(BUILD_DIR)/raster 2_raster.cpp resources/tut_raster.cpp $(CXXFLAGS) $(LDFLAGS_SDL) 
	$(CXX) -o $(BUILD_DIR)/sound 3_sound.cpp resources/power_on_wav.cpp $(CXXFLAGS) $(LDFLAGS_SDL) 