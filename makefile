all:
	cc vidtogfx.c -o vidtogfx -lm -lc -lX11 -lGL -lML -lMLU -laudio
	cc memtovid.c -o memtovid -lm -lc -lX11 -lGL -lML -lMLU -laudio


