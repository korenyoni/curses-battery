make:
	g++ battery-draw.cpp -o battery-draw -lncursesw -std=gnu++11 -g
clean:
	rm battery-draw
