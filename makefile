all:	vidtogfx memtovid controls

vbob.o: vbob.cpp makefile
	g++ --std=c++11 vbob.cpp -c vbob.o 

vidtogfx: vidtogfx.cpp vbob.o makefile
	g++ --std=c++11 vidtogfx.cpp vbob.o -o vidtogfx -lm -lc -lX11 -lGL -lML -lMLU -laudio

memtovid: memtovid.cpp vbob.o makefile
	g++ --std=c++11 -O3 memtovid.cpp vbob.o -o memtovid -lm -lc -lML -lMLU

controls: controls.cpp vbob.o makefile
	g++ --std=c++11 controls.cpp vbob.o -o controls -lm -lc -lML -lMLU

clean:
	rm -f controls
	rm -f memtovid
	rm -f vidtogfx
	rm -rf ii_files

