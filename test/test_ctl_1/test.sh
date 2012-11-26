cd ../../
make clean
make

cd ./test/test_ctl_1
cp ../../peer client/peer

mkdir -p server
cp ../../peer server/peer

../../hupsim.pl -m config/topo.map -n config/nodes.map -p 12345 -v 0 &
SIM_PID=$!
export SPIFFY_ROUTER=127.0.0.1:12345

cd server
./peer -p ../config/nodes.map -c ../config/B.haschunks -m 4 -i 2 -f ../config/C.masterchunks &
SERVER_PID=$!
cd ..

cd client
./peer -p ../config/nodes.map -c ../config/A.haschunks -m 4 -i 1 -f ../config/C.masterchunks < input.txt &
CLIENT_PID=$!
cd ..

wait $CLIENT_PID
kill -9 $SIM_PID
wait $SIM_PID
kill -9 $SERVER_PID

diff -s client/TEST.tar ../../config/B.tar

grep ^1 server/problem2-peer.txt > data.dat
gnuplot config/plot.script

rm client/TEST.tar
rm data.dat
rm client/problem2-peer.txt
rm client/peer
rm -rf server
