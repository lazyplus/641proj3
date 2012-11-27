cd ../../
make clean
CFLAG=-DTERM_FIN make

cd ./test/test_rb_3
cp ../../peer client1/peer
cp ../../peer client2/peer

mkdir -p server
cp ../../peer server/peer

../../hupsim.pl -m config/topo.map -n config/nodes.map -p 12345 -v 0 &
SIM_PID=$!
export SPIFFY_ROUTER=127.0.0.1:12345

cd server
./peer -p ../config/nodes.map -c ../config/C.haschunks -m 1 -i 3 -f ../config/C.masterchunks &
cd ..

cd client1
./peer -p ../config/nodes.map -c ../config/A.haschunks -m 4 -i 1 -f ../config/C.masterchunks < input.txt &
CLIENT_PID1=$!
cd ..

sleep 5

cd client2
./peer -p ../config/nodes.map -c ../config/B.haschunks -m 4 -i 2 -f ../config/C.masterchunks < input.txt &
CLIENT_PID2=$!
cd ..

kill -9 $CLIENT_PID1
wait $CLIENT_PID1
wait $CLIENT_PID2
kill -9 $SIM_PID
wait $SIM_PID
killall peer

diff -s client2/TEST.tar ../../config/B.tar

grep ^1 server/problem2-peer.txt > data.dat
gnuplot config/plot.script
grep ^2 server/problem2-peer.txt > data.dat
gnuplot config/plot.script
rm data.dat

rm client1/TEST.tar
rm client2/TEST.tar
rm client1/problem2-peer.txt
rm client2/problem2-peer.txt
rm client1/peer
rm client2/peer
rm -rf server
