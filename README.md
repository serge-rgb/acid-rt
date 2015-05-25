Acid Ray Tracer
===============

Real time ray tracing renderer
------------------------------

![screenshot](http://i.imgur.com/kyQswNi.png)

This is a real time ray tracing renderer; a proof of concept designed to be work with the Oculus Rift.

As of SDK 0.6.0, this project is no longer maintained.
------------------------------------------------------

Features:
---------

* OpenCL ray tracer for triangle meshes.
* Simple lambertian material... (texture mapping & materials soon)
* Crappy OBJ loader
* Screen-space antialiasing with FXAA

VR Features:
------------

* Only works in extended mode, for now.
* Lens distortion correction
* Chromatic aberration correction
* Timewarp

Requirements:
-------------

* Oculus DK2 on Windows.
* OpenCL 1.1
* OpenGL 4.1
* A 64-bit processor

How to build
------------

You will need:
* Windows (Tested on Win7 x86_64 with NVidia 770 GPU)
* Git for Windows (with Git Shell)
* CMake
* Visual Studio 2013
* OpenCL runtime. On NVIDIA, this means installing the latest CUDA Toolkit.

Place the latest (0.4.4) OculusSDK folder inside the project's root directory.

Open git shell, go to the project dir and run:

    $> ./get_stuff.sh

... wait until it's done.

Open a Windows Command Prompt and run Visual Studio's `vcvarsall.bat` script.
With Visual Studio 2013, this is usually:

    "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" amd64

Make a directory called "build"

    $> mkdir build

Use cmake to create a 64 bit visual studio solution.

    $> cd build && cmake -G "Visual Studio 12 2013 Win64" ..

Run `build.bat` to build the project. It's recommended to do it at least the first time, so that it copies the necessary DLLs into the target's directory.

Run samples
-----------

Build and run the 'samples' project. Left and right arrows switch between samples.

1. Cube grid
2. Stanford Bunny
3. Crytek Sponza

If running from Visual Studio, set `Samples` as "startup project" and in the project properties, append `\..\..\..` to the working directory.

License
-------

BSD-style.

To-Do (in no particular order)
------------------------------

* Mesh loader (find third party lib)
* Lighting model
* Texture mapping (?)
