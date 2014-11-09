Acid Ray Tracer
===============

Real time ray tracing renderer
------------------------------

This is what it was looking like before the port to OpenCL (in-progress).

![old screenshot](http://i.imgur.com/iZVTZX4.png)

This is a real time ray tracing renderer, designed to be a drop-in substitute for the Oculus Rift's SDK renderer.

This is a work in progress. Not for general consumption, but the aim is to be able to make games using it.

Features:
---------

* Triangle soup renderer.
* GLSL compute shader ray tracer. (Being replaced by OpenCL in the opencl branch)
* Simple lambertian material, texture mapping & materials will be implemented.
* Crappy OBJ loader (will be replaced by third party lib)

VR Features:
------------

* Lens distortion correction
* Chromatic aberration correction

Requirements:
-------------

* Only Supports DK2 on Windows.
* Requires OpenGL 4.3  (Will change soon)
* Requires OpenCL 1.1

How to build
------------

You will need:
* Windows (Tested on Win7 x86_64 with NVidia 770 GPU)
* Git for Windows (with Git Shell)
* CMake
* Visual Studio 2013

Place the latest (0.4.3) OculusSDK folder inside the project's root directory.

Open git shell, go to the project dir and run:

    $> ./get_stuff.sh

... wait until it's done.

 Make a directory called "build" (can't be another name)

    $> mkdir build

Use cmake to create a visual studio solution.

    $> cd build && cmake -G "Visual Studio 12" ..

... or you can use the CMake GUI

Run samples
-----------

Build and run the 'samples' project. Left and right arrows switch between samples.

1. Cube grid
2. Stanford Bunny
3. Crytek Sponza

You can do that either through Visual Studio or through the built-in
`build.bat` script. `build.bat` copies DLLs so it should be run at least once
for each config.

License
-------

BSD-style.

To-Do (in no particular order)
------------------------------

* OpenCL rendering.
* Mesh loader
* Lighting model
* BRDF materials
* On-screen text
* Desktop mode
* Direct3D backend
* Time Warp

