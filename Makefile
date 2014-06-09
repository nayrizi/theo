theo: main.cpp
	g++ -IC:\boost\boost_1_55_0\ -c main.cpp -o main.o -std=c++11 -O2 -ggdb -static -lmingw32
	g++ -o theo main.o C:\boost\boost_1_55_0\stage\lib\*.a
	
	