CPP=g++
INCS=-Ivendor/miniaudio -Ivendor/leif/include -Ivendor/leif/vendor/glad/include
LIBS=-lleif -Lvendor/leif/lib -lglfw -ltag -lm -Lvendor/miniaudio/lib -lminiaudio
CFLAGS=-O3 -ffast-math -DGLFW_INCLUDE_NONE 

LEIF_LIB_DIR := ./vendor/leif/lib/

.PHONY: check-leif leif build 

all: check-leif build  

check-leif:
	@if [ ! -d $(LEIF_LIB_DIR) ]; then \
		echo "[INFO] Building leif."; \
        $(MAKE) leif; \
    else \
		echo "[INFO]: Leif already built."; \
    fi

leif:
	$(MAKE) -C ./vendor/leif/

build: bin
	@echo "[INFO]: Building Lyssa."
	${CPP} ${CFLAGS} src/*.cpp -o bin/lyssa ${INCS} ${LIBS} 

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

