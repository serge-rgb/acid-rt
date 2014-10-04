clang++ pthread.cc -O0 -pthread -o pthread

if [ $? -ne 0 ]; then
    exit $?
else
    ./pthread
fi
