
**forked from umd-memsys/DRAMsim3**

## Build
```
mkdir build
cd build
cmake -DCMAKE_CXX_STANDARD=17 ..
make
```

## Test
```
cd lab
g++ -std=c++17 testDramSim.cpp -I../src -L.. -ldramsim3 -o testDramSim
export LD_LIBRARY_PATH=../:$LD_LIBRARY_PATH
```