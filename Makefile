CPP=g++
INCS=-Ivendor/miniaudio -Ivendor/leif/include -Ivendor/leif/vendor/glad/include -Ivendor/stb_image_write
LIBS=-lleif -lclipboard -Lvendor/leif/lib -lglfw -lm -Lvendor/miniaudio/lib -lminiaudio -lxcb
PKG_CONFIG=`pkg-config --cflags --libs taglib`
CFLAGS=-O3 -ffast-math -DGLFW_INCLUDE_NONE -std=c++17

LEIF_LIB_DIR := ./vendor/leif/lib/
LYSSA_DIR := ~/.lyssa/

.PHONY: check-leif leif build 

all: check-leif build  

check-leif:
	@if [ ! -d $(LEIF_LIB_DIR) ]; then \
		echo "[INFO] Building leif."; \
        $(MAKE) leif; \
    else \
		echo "[INFO]: Leif already built."; \
    fi

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

leif:
	$(MAKE) -C ./vendor/leif/

build: bin lyssa-dir
	@echo "[INFO]: Building Lyssa."
	${CPP} ${CFLAGS} src/*.cpp -o bin/lyssa ${INCS} ${LIBS} ${PKG_CONFIG} 

bin:
	mkdir bin

clean:
	rm -rf ./bin 
	rm -rf ./vendor/leif/lib 

run: 
	cd ./bin && ./lyssa

rebuild:
	$(MAKE) clean
	$(MAKE) leif
	$(MAKE) build

install:
	$(MAKE) -C ./vendor/leif/ install
	cp ./bin/lyssa /usr/bin/
	cp ./Lyssa.desktop /usr/share/applications
	cp -r ./logo /usr/share/icons/lyssa

uninstall:
	$(MAKE) -C ./vendor/leif/ uninstall
	rm -rf $(LYSSA_DIR)
	rm -rf /usr/bin/lyssa 
	rm -rf /usr/share/applications/Lyssa.desktop
	rm -rf /usr/share/icons/lyssa/
