rm -rf build-asan;
cmake -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSOMECHOPS_SANITIZE_ADDRESS=ON;
cmake --build build-asan --config RelWithDebInfo --parallel 4
