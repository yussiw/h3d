H3D Engine

![Language](https://img.shields.io/github/languages/top/yussiw/h3d?style=for-the-badge)
![Languages](https://img.shields.io/github/languages/count/yussiw/h3d?style=for-the-badge)
![License](https://img.shields.io/badge/License-MIT-brightgreen?style=for-the-badge)

<p align="center">
  <strong>A lightweight 3D engine for creating interactive experiences.</strong>
</p><p align="center">
  <img src="https://img.shields.io/badge/Engine-H3D-7289DA?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Scripting-Lua%205.4-blue?style=for-the-badge" />
  <img src="https://img.shields.io/badge/3D-Real--Time-8A2BE2?style=for-the-badge" />
</p>---

About

H3D is a 3D engine designed to make game development more accessible through a simple scripting interface and support for external 3D assets.

Built around Lua 5.4, H3D allows developers to create interactive scenes, control objects, and bring 3D models to life without having to deal with complicated engine internals.

Features

- 3D Model Importing — Load external models using OBJ, GLB and GLTF.
- Texture Support — Automatically load and display model textures.
- Animation Playback — Play animations included in imported models, including skeletal animations.
- High Polygon Capacity — Supports models with up to 650,000 vertices, with optimization.
- Lua 5.4 Scripting — Control your scenes using Lua.
- Object Access — Find and manipulate scene objects through scripts.
- Real-Time Updates — Execute game logic every frame.

Scripting

H3D uses Lua 5.4 as its scripting language.

Scripts can define two main functions:

function start()
    -- Called when the game starts
end

function update(dt)
    -- Called every frame
end

Example

local cube, sphere

velocidade = 90

function start()
    cube = find("Cubo 1")
    sphere = find("Esfera 1")
end

function update(dt)
    -- Your game logic goes here
end

Supported 3D Formats

Format| Support
OBJ| Supported
GLB| Supported
GLTF| Supported

Imported models can contain textures and animations, which H3D can load and play.

Project Goals

H3D aims to provide a practical development environment for creating 3D games and interactive projects, combining accessible scripting with the ability to work with external 3D assets.

Developer

Created by "@yussiw" (https://github.com/yussiw).

---

<p align="center">
  <strong>H3D Engine</strong><br/>
  Made with passion for game development.
</p>
