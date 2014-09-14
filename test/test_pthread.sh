clang++ pthread.cc -pthread -o pthread

if [ $? -ne 0 ]; then
    exit $?
else
    ./pthread
fi
