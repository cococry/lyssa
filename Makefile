CPP=g++
INCS=-Ivendor/miniaudio -Ivendor/leif/vendor/glad/include -Ivendor/stb_image_write
LIBS=-lleif -lclipboard -lleif -lglfw -lm -Lvendor/miniaudio/lib -lminiaudio -lxcb -lGL
PKG_CONFIG=`pkg-config --cflags --libs taglib`
CFLAGS=-O3 -ffast-math -DGLFW_INCLUDE_NONE -std=c++17

LYSSA_DIR=~/.lyssa/

all: build 

lyssa-dir: 
	@mkdir -p $(LYSSA_DIR)
	cp -r ./scripts/ $(LYSSA_DIR)
	cp -r ./assets/ $(LYSSA_DIR)
	@if [ ! -d ~/.lyssa/playlists/ ]; then \
		mkdir ~/.lyssa/playlists; \
	fi
	@if [ ! -d ~/.lyssa/downloaded_playlists/ ]; then \
		mkdir ~/.lyssa/downloaded_playlists; \
	fi
	if [ ! -d ~/.lyssa/playlists/favourites ]; then \
		cp -r ./.lyssa/favourites/ ~/.lyssa/playlists; \
	fi

build: bin lyssa-dir
	@echo "[INFO]: Building Lyssa."
	${CPP} ${CFLAGS} src/*.cpp -o bin/lyssa ${INCS} ${LIBS} ${PKG_CONFIG} 

bin:
	mkdir bin

clean:
	rm -rf ./bin 

run: 
	cd ./bin && ./lyssa

install:
	cp ./bin/lyssa /usr/bin/
	cp ./Lyssa.desktop /usr/share/applications
	cp -r ./logo /usr/share/icons/lyssa

uninstall:
	rm -rf $(LYSSA_DIR)
	rm -rf /usr/bin/lyssa 
	rm -rf /usr/share/applications/Lyssa.desktop
	rm -rf /usr/share/icons/lyssa/
