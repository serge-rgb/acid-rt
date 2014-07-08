make -j tardis

if [ $? -ne 0 ]; then
    exit $?
else
    ./tardis/tardis
fi
