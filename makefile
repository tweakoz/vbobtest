all:
	cc vidtogfx.c -o vidtogfx -lm -lc -lX11 -lGL -lML -lMLU -laudio
