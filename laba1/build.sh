mkdir _build
cd _build
g++ -c -Wall -g ../hw.cpp
g++ -c -Wall -g ../func.cpp
g++ -g hw.o func.o -o hw.exe
cd ..