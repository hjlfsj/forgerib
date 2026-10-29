#!/bin/bash

./build/bin/crush_vme $(($1-1))
# ./build/bin/sift -x $1 -v $(($1-1)) > log/sift_$1.txt
# ./build/bin/coke -x $1 -v $(($1-1)) > log/coke_$1.txt
./build/bin/smelt -x $1 -v $(($1-1)) -t t1 tafd t1du t1dd
# ./build/bin/smelt -x $1 -v $(($1-1)) -t main t0d1 t0d2 t0d3 t0d4 t0s t0csi ppac beam > log/smelt_main_$1.txt
