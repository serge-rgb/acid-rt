# NAME

NAME is a VR stealth platformer game.

## Mechanics (BROAD):

Although the universe is 3D, gameplay occurs in a plane -- That is, the center
of all objects is always inside a certain plane facing the sitting user. This
allows for better platformer mechanics, which suffer when taken to 3D. It also
facilitates level creation greatly. Better and more levels in the same time
compared to fully-3D, which would be infeasible.

## Style (BROAD):

No simulation of our universe, i.e. no Lambert function, no BRDFs. Light exists,
but there is complete artistic freedom w.r.t. how it interacts with objects.

Low-polygon assets are preferred because of lack of time, talent and resources.
However, we can use this to our advantage. Low poly gives us creative mobility
because we are not constrained by the pursuit of realism.

## Tech

Ray tracing, mainly because it's fun. This being a stealth game; we benefit from
having relatively cheap, simple hard shadows. The rough plan is to keep an easily
updated acceleration structure, and if that proves too slow, keep an array of
separate bounding boxes for animated objects.

## Platform

NAME is intended to be a Steam game. Current target platforms are Linux
(Debian derivatives and SteamOS) and Windows. OS X is not a possibility
(lack of OpenGL 4.3), and if it was, it would still not be a target, unless
research proves that it is an important market. The reason is that maintaining
multiple platforms is a pain, and SteamOS seems like the most viable target
platform.
NAME is currently a VR only game. Every gameplay decision will be influenced by
it.

