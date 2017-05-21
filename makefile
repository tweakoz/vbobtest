vidtogfx:	vidtogfx.cpp
	g++ vidtogfx.cpp -o vidtogfx -lm -lc -lX11 -lGL -lML -lMLU -laudio

memtovid:	memtovid.cpp
	g++ memtovid.cpp -o memtovid -lm -lc -lX11 -lGL -lML -lMLU -laudio

controls:	controls.cpp
	g++ controls.cpp -o controls -lm -lc -lX11 -lGL -lML -lMLU -laudio

clean:
	rm -f controls
	rm -f memtovid
	rm -f vidtogfx
	rm -rf ii_files

