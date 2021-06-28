if [ ! -d "build" ];
then
    mkdir "build"
fi

cd "build"

cmake ..
make

IP="192.168.5.241"
PORT=1337
CMD_PORT=1338
FILE="FireBaseCurl.self"

echo destroy | nc  $IP $CMD_PORT
mv $FILE eboot.bin
ncftpput -P $PORT -u Anonymous -p b0ss $IP "/ux0:/app/AAAA11111" eboot.bin
#echo "file ux0:data/$FILE" | nc $IP $CMD_PORT
echo launch AAAA11111 | nc $IP $CMD_PORT