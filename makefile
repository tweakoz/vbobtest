all:	vidtogfx memtovid controls

vidtogfx: vidtogfx.cpp
	g++ --std=c++11 vidtogfx.cpp -o vidtogfx -lm -lc -lX11 -lGL -lML -lMLU -laudio

memtovid: memtovid.cpp makefile
	g++ --std=c++11 -O3 memtovid.cpp -o memtovid -lm -lc -lML -lMLU

controls: controls.cpp
	g++ --std=c++11 controls.cpp -o controls -lm -lc -lML -lMLU

clean:
	rm -f controls
	rm -f memtovid
	rm -f vidtogfx
	rm -rf ii_files

