#!/bin/bash
# crush
./build/bin/crush/crush_ppac -c config_14O.toml $1
./build/bin/crush/crush_beam -c config_14O.toml $1
./build/bin/crush/crush_trigger -c config_14O.toml $1
./build/bin/crush/crush_t0d1 -c config_14O.toml  $1
./build/bin/crush/crush_t0d2 -c config_14O.toml $1
./build/bin/crush/crush_t0d3 -c config_14O.toml $1
./build/bin/crush/crush_t0d4 -c config_14O.toml $1
./build/bin/crush/crush_others -c config_14O.toml $1
./build/bin/crush_vme -c config_14O.toml $1
./build/bin/sift -c config_14O.toml -x $1 -v $1 > log/sift_trigger_$1.txt
./build/bin/coke -c config_14O.toml -r $1
./build/bin/smelt -c config_14O.toml -r $1 -v $1 -t t1 t0d1 t0d2 t0d3 t0d4 t0s t0csi ppac tafd tafcsi t1csiu t1csid t1du t1dd t1sd t1su beam
./build/bin/smelt -c config_14O.toml -r $1 -v $1 -t main t0d1 t0d2 t0d3 t0d4 t0s t0csi ppac beam
