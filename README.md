Acid Ray Tracer
===============

Real time ray tracing renderer
------------------------------

This is what it was looking like before the port to OpenCL (in-progress).

![old screenshot](http://i.imgur.com/iZVTZX4.png)

This is a real time ray tracing renderer with support for the Oculus Rift.
It was based on GLSL compute shaders but now those parts are being ported to OpenCL.


Features / Requirements:
------------------------

* WIP -- Not for general consumption at the moment :)
* Young, ray tracing polygon renderer with some samples included.
* Only Supports DK2 on Windows.
* Requires OpenGL 4.3

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

TODO
----

* port to OpenCL
* use external mesh loader
* lighting model
* on screen text
* desktop mode
