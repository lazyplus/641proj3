cd ../../
make clean
CFLAG=-DTERM_FIN make

cd ./test/test_p2p_3
cp ../../peer client1/peer
cp ../../peer client2/peer

mkdir -p server1
mkdir -p server2
cp ../../peer server1/peer
cp ../../peer server2/peer

../../hupsim.pl -m config/topo.map -n config/nodes.map -p 12345 -v 0 &
SIM_PID=$!
export SPIFFY_ROUTER=127.0.0.1:12345

cd server1
./peer -p ../config/nodes.map -c ../config/A.haschunks -m 4 -i 1 -f ../config/C.masterchunks &
cd ..

cd server2
./peer -p ../config/nodes.map -c ../config/B.haschunks -m 4 -i 2 -f ../config/C.masterchunks &
cd ..

cd client1
./peer -p ../config/nodes.map -c ../config/C.haschunks -m 4 -i 3 -f ../config/C.masterchunks < input.txt &
CLIENT_PID1=$!
cd ..

cd client2
./peer -p ../config/nodes.map -c ../config/D.haschunks -m 4 -i 4 -f ../config/C.masterchunks < input.txt &
CLIENT_PID2=$!
cd ..

wait $CLIENT_PID1
wait $CLIENT_PID2
kill -9 $SIM_PID
wait $SIM_PID
killall peer

diff -s client1/TEST.tar ../../config/B.tar
diff -s client2/TEST.tar ../../config/B.tar

grep ^1 server1/problem2-peer.txt > data.dat
gnuplot config/plot.script
grep ^1 server2/problem2-peer.txt > data.dat
gnuplot config/plot.script
rm data.dat

rm client1/TEST.tar
rm client2/TEST.tar
rm client1/problem2-peer.txt
rm client2/problem2-peer.txt
rm client1/peer
rm client2/peer
rm -rf server1
rm -rf server2
