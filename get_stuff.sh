echo "========================================================================="
echo "==== Get stuff."
echo "========================================================================="


# Get deps
# sudo apt-get install autogen libtool libatomic-ops-dev unzip

# Update third party libraries.
if [ ! -d third_party ]; then
    mkdir third_party
fi

cd third_party

echo "========================================================================="
echo "==== GLFW3"
echo "========================================================================="
if [ ! -d glfw ]; then
    git clone https://github.com/glfw/glfw.git glfw
else
    cd glfw
    git pull
    cd ..
fi

if [ ! -d glm ]; then
echo "========================================================================="
echo "==== GLM"
echo "========================================================================="
    unzip ../glm-0.9.5.4.zip
fi

echo "========================================================================="
echo "==== stb"
echo "========================================================================="
if [ ! -d stb ]; then
    git clone https://github.com/nothings/stb.git
else
    cd stb
    git pull
    cd ..
fi
cd ..  # root

if [ -d OculusSDK ]; then
echo "========================================================================="
echo "==== OVR"
echo "========================================================================="
    cd OculusSDK/LibOVR
    make -j
    cd ../..
fi

echo "Done."

