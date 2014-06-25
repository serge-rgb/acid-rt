echo "========================================================================="
echo "==== Get stuff."
echo "========================================================================="


# Get deps
sudo apt-get install autogen libtool libatomic-ops-dev
# LUA
sudo apt-get install libreadline-dev

# Update third party libraries.
if [ ! -d third_party ]; then
    mkdir third_party
fi

cd third_party

echo "========================================================================="
echo "==== Boehm GC"
echo "========================================================================="
if [ ! -d bdwgc ]; then
    git clone https://github.com/ivmai/bdwgc
    cd bdwgc
    git pull
    ./autogen.sh
    ./configure --prefix=`pwd`/..
    cd ..
fi
cd bdwgc
make -j
make install
cd ..

echo "========================================================================="
echo "==== GLFW3"
echo "========================================================================="
if [ ! -d third_party ]; then
    git clone https://github.com/glfw/glfw.git glfw
fi

echo "========================================================================="
echo "==== Lua"
echo "========================================================================="
if [ ! -d lua ]; then
    git clone https://github.com/LuaDist/lua.git
else
    echo "Done."
fi
cd ..  # root




