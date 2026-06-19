#!/bin/bash
# crush
./build/bin/crush/crush_ppac $1
./build/bin/crush/crush_beam $1
./build/bin/crush/crush_trigger $1
./build/bin/crush/crush_t0d1 $1
./build/bin/crush/crush_t0d2 $1
./build/bin/crush/crush_t0d3 $1
./build/bin/crush/crush_t0d4 $1
./build/bin/crush/crush_others $1
./build/bin/crush_vme -v $1
./build/bin/sift -x $1 -v $1 > log/sift_trigger_$1.txt
./build/bin/coke -r $1 -v $1
./build/bin/smelt -r $1 -v $1 -t t1 t0d1 t0d2 t0d3 t0d4 t0s t0csi ppac tafd tafcsi t1csiu t1csid t1du t1dd t1sd t1su beam
./build/bin/smelt -r $1 -v $1 -t main t0d1 t0d2 t0d3 t0d4 t0s t0csi ppac beam


