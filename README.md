# 🌌 ShaderViewerX11

**ShaderViewerX11** is a C/OpenGL/GLX-based shader renderer built on top of the X11 windowing system for Linux. It supports live interaction with user-defined GLSL shaders using a configuration system and real-time uniform updates. This project is designed for performance testing, debugging, or visual experimentation with fragment/vertex shader pipelines.

---

## ✨ Features

- 🔷 Lightweight OpenGL 3.3 core context
- 🖥️ Renders fullscreen OpenGL scenes using X11
- 🔧 Configurable shader loading via `.ini`-style config file
- 🎛️ Real-time uniform control for time, resolution, mouse, keyboard, and other system states
- 🖼️ Texture loading via `stb_image`
- 🎮 Input handling (keyboard/mouse) via X11 events
- 🌐 Home-directory expansion (`~`) in file paths

---

## 🧰 Dependencies

| Dependency     | Purpose                         |
|----------------|---------------------------------|
| `glad`         | OpenGL function loader          |
| `stb_image`    | Image loading (textures)        |
| `nuklear`      | (Currently unused GUI stub)     |
| `X11`          | Linux window/input system       |
| `GLX`          | OpenGL X11 context              |
| `parser.h`     | Custom INI-style config parser  |

> Ensure you have OpenGL 3.3+ drivers and development headers installed.

---

## 📦 Build Instructions

```bash
sudo apt install libx11-dev libgl1-mesa-dev libglx-dev
make
```

---

## 🧪 Running the Application

```bash
./shaderviewer <config_file.ini>
```

Example:

```bash
./shaderviewer configs/demo.ini
```

If no configuration is supplied, a fallback colored triangle is rendered.

---

## 🔧 Configuration File Format

The configuration file is a simple `.ini`-style layout with the following keys:

```ini
[general]
shadermode=shader

[shadermode/shader]
vertexshader=shaders/vertex.glsl
fragmentshader=shaders/fragment.glsl
```

Paths can use `~` for the home directory.

---

## 🕹️ Controls & Inputs

| Key / Input      | Mapped Index      |
|------------------|-------------------|
| `W` / `A` / `S` / `D` | 0–3            |
| `Space`          | 4                 |
| `Left Shift`     | 5                 |
| `Left Ctrl`      | 6                 |
| `Esc`            | 7                 |
| Mouse Buttons    | 29–31             |
| Scroll Up/Down   | Adds/Subtracts `scrollDelta` |

These indices are used to populate uniform arrays like `iKeyStates`, `iJoyStates`, etc.

---

## 🔮 Available Uniforms

The following uniforms are automatically updated and passed into your shader:

- `iTime` – Time since start (seconds)
- `iResolution` – (width, height, 1.0)
- `iMouse` – Mouse position (x, y)
- `iZoom`, `iScroll`, `iBattery`, `iVolume`, `iMaxVolume`
- `iCameraPosition`, `iCameraVelocity`
- `iKeyStates[32]`, `iJoyStates[32]`, `iSampleStates[128]`
- `iUserTextures[32]` – Bound texture units

---

## 🧹 Cleanup

Gracefully disposes of:

- Shader programs
- GL mesh (VBO/VAO)
- Texture memory
- X11 display/context

---

## 🔬 Development Notes

- Designed as a base for real-time shader experimentation or game prototype subsystems.
- No GUI provided yet, though `nuklear.h` is included as a possible future direction.
- The system honors performance (GPU-side) and modularity (via session configuration abstraction).

---

## 🙏 Acknowledgments

- **GLAD** – for loading modern OpenGL
- **Sean Barrett (`stb_image`)** – image loader
- **Nuklear** – simple UI library (placeholder for future)
- **X11 & GLX** – Linux windowing and OpenGL context

---

## 📜 License

MIT License (or as you choose to declare in the repository).

---

May your shaders compile clean, your framebuffers stay clear, and your rendering loop spin forever in perfect sync. 🌠
