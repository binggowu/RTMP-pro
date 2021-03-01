# `pkg-config --libs --cflags libavformat libavdevice libavcodec libavutil libswresample libavfilter  libpostproc libswscale`

ch04: ch04.cpp
	g++ -g $^ -o $@ `pkg-config --libs --cflags libavformat libavdevice libavcodec libavutil libswresample libavfilter  libpostproc libswscale`