cd ../../
make clean
make

cd ./test/test_p2p_1
cp ../../peer client/peer
cp ../../peer server/peer

../../hupsim.pl -m config/topo.map -n config/nodes.map -p 12345 -v 0 &
SIM_PID=$!
export SPIFFY_ROUTER=127.0.0.1:12345

mkdir -p server
cd server
./peer -p ../config/nodes.map -c ../config/B.haschunks -m 4 -i 2 -f ../config/C.masterchunks &
./peer -p ../config/nodes.map -c ../config/C.haschunks -m 4 -i 3 -f ../config/C.masterchunks &
cd ..

cd client
./peer -p ../config/nodes.map -c ../config/A.haschunks -m 4 -i 1 -f ../config/C.masterchunks < input.txt &
CLIENT_PID=$!
cd ..

wait $CLIENT_PID
kill -9 $SIM_PID
wait $SIM_PID
killall peer

diff -s client/TEST.tar ../../config/B.tar

rm client/TEST.tar
rm client/problem2-peer.txt
rm server/problem2-peer.txt
rm client/peer
rm server/peer
