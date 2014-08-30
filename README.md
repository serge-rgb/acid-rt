# nuwen

## Real time ray tracing library for Virtual Reality.

I'm writing a game with the Oculus Rift in mind, and I decided to make the guts open source.

![old screenshot](http://bigmonachus.org/img/c_log_6_1.png)

## Features / Requirements:

* Not for general consumption at the moment :)
* Includes a young, ray tracing polygon renderer and a test scene.
* (Only) Supports DK2 on Windows.
* OpenGL 4.3 required
* Written in modern C, with some features taken from C++
    - namespaces, auto, polymorphism, templates for generics when absolutely needed.
    - standard library inspired by Go and Haskell
    - GC when it makes sense.

## Planned:

* Loader for animated meshes
* Integration with some sound library
* Computer art (fractal 3D structures and a synth)
* Linux support as soon as Oculus releases Linux libs for SDK > 0.4 

## How to build

You will need:
* Windows (Tested on Win7 x86_64 with NVidia 770 GPU)
* Git for Windows (with Git Shell)
* CMake

Place the latest (0.4.1) OculusSDK folder inside the root directory.

Open git shell and run:

    $> ./get_stuff.sh
    
... wait until it's done and then make a directory called "build" (can't be another name)

    $> mkdir build
    
Use cmake to create a visual studio solution.
    
    $> cd build && cmake -G "Visual Studio 12" ..

...or you can use the CMake GUI

Build the 'cubes' project to test the ray tracer.

# FAQ 

## What can I use this for???

Nothing! It you be great if you tell me the ways in which my ray tracer sucks and how to make it faster. 
Eventually it should be a nice, lightweight, library for interactive software.

## Why write everything from scratch?

Because it's fun. And because it lets me code in my own superset of C that feels a lot like Go.

## License?

I would think BSD-style. 
