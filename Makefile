LSC = ./node_modules/livescript/bin/lsc
LS_SRC_FILES = src/test.ls src/mmap-io.ls

all: update_modules build

update_modules:
	npm update

build: ls build_addon

build_addon:
	node-gyp configure && node-gyp rebuild

human_errors:
	clang++ --std=c++11 -S -I node_modules/nan/ -I /usr/local/include/node src/mmap-io.cc

ls: $(LS_SRC_FILES:src/%.ls=%.js)

%.js: src/%.ls
ifeq ($(shell test -e ../../$(LSC) && echo -n yes),yes) #uses info from https://stackoverflow.com/questions/5553352/how-do-i-check-if-file-exists-in-makefile
	echo "using parent's LiveScript"
	../../$(LSC) -b -c -o ./ $<
else
	$(LSC) -b -c -o ./ $<
endif

clean:
	rm -rf build *.js

watch:
	while true; do (make build; inotifywait -qre close_write,moved_to --exclude '\.git' ./src/; ) done;
