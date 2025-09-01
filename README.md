# 🛫 myProject3DPlaneSimulator - Version 1.6

A 3D plane simulation demo built using **OpenGL ES 2.0**, simulating terrain rendering, dynamic minimap, skybox visuals, and a plane flying over a height-mapped world.


## 🚀 Features

- 🌍 Procedural terrain generation using heightmaps
- 🌫️ Fully textured **skybox** environment
- 🛩️ Simulated 3D **plane object** flying over the terrain
- 🗺️ **Minimap** showing real-time plane position
- 🧾 On-screen text rendering (FPS counter, position, etc.)
- 📦 Simple OBJ loader using `tinyobj_loader`

## 🛠️ Requirements

- **GLFW 3**
- **OpenGL ES 2.0**
- **cglm library**
- **text.h, text.c**
- **tinyobj_loader.h, tinyobj_loader.c**
- **for model -> .obj, .mtl, .png -> .png can be changed from .mtl file There is options**
- **C compiler** (tested with GCC)


# Project Structure
myProject3DPlaneSimulator/
├── include/
│   ├── cglm/                    # cglm math library headers
│   ├── models/                  # 3D model files (.obj, .mtl, .png)
│   ├── skybox/                  # Skybox textures
│   ├── text/                    # Text rendering headers
│   ├── globals.h                # Global definitions and constants
│   ├── heightMap.h              # Terrain generation header
│   ├── heightMapImage.h         # Image parsing for heightmap
│   ├── heightMapImage.png       # Heightmap texture
│   ├── minimap.h                # Minimap header
│   ├── plane.h                  # Plane model and logic
│   ├── skybox.h                 # Skybox rendering header
│   ├── textShowing.h            # In-game text display header
│   └── tiny_obj_loader.h        # Tiny OBJ loader header
│   └── stb_image.h              # Stb image loader
├── src/
│   ├── heightMap.c              # Terrain rendering logic
│   ├── minimap.c                # Minimap implementation
│   ├── plane.c                  # Plane simulation and controls
│   ├── skybox.c                 # Skybox rendering logic
│   ├── textShowing.c            # In-game text rendering
│   ├── tiny_obj_loader.c        # OBJ file loader implementation
│   └── main.c                   # Entry point of the simulation
└── README.md                    # Project documentation



### Linux Build Example (GCC):

```bash
gcc -o myProject3DPlaneSimulator \
    src/text/text.c \
    src/heightMap/Final3DVersion/main.c \
    src/heightMap/Final3DVersion/heightMap.c \
    src/heightMap/Final3DVersion/minimap.c \
    src/heightMap/Final3DVersion/plane.c \
    src/heightMap/Final3DVersion/skybox.c \
    src/heightMap/Final3DVersion/textShowing.c \
    src/heightMap/Final3DVersion/tinyobj_loader.c \
    -lGLESv2 -lglfw -lm -ldl
```

## 🎮 Controls

# To activate controls make KEYBOARD_ENABLED 1 in top of main.c

# Camera Views
1 - Rear Chase Camera   
2 - Cockpit View   
3 - High Altitude Rear View  
4 - Rear Zoomed-Out View   
5 - Right Side View   
6 - Left Side View    
7 - Rear Below View   
8 - Top Diagonal View   

# Plane Controls
Arrow Keys     - Control Pitch, Yaw, and Roll    
Shift          - Speed Up (Accelerate)    
S              - Toggle Speed Lock    
Z              - Zoom In (Hold)    
A              - Toggle Autopilot Mode   
T              - Toggle Grid View Mode (Triangle Wireframe)    
ESC            - Exit Simulation    

# Crash Recovery   
R              - Restart Flight After Crash   
C              - Teleport Back to Crash Site (Safe Altitude)   