nuwen
=====

Real time ray tracing library for the Oculus Rift.
--------------------------------------------------

![old screenshot](http://bigmonachus.org/img/c_log_6_1.png)

Features / Requirements:
------------------------

* Not for general consumption at the moment :)
* Young, ray tracing polygon renderer and some samples.
* Only Supports DK2 on Windows.
* OpenGL 4.3 required

How to build
------------

You will need:
* Windows (Tested on Win7 x86_64 with NVidia 770 GPU)
* Git for Windows (with Git Shell)
* CMake
* Visual Studio 2013

Place the latest (0.4.2) OculusSDK folder inside the project's root directory.

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

Build and run the 'samples' project.

You can do that either through Visual Studio or through the built-in
`build.bat` script. `build.bat` copies DLLs so it should be run at least once
for each config.


License
-------

BSD-style.

TODO
----

* crytek sponza
* rudimentary lighting model
* bouncing ball sample
* on screen text
* non-vr mode (fallback)
