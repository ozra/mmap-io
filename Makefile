all: compile
	node-gyp configure && node-gyp build

compile:
	lsc -bco . src

clean:
	rm -rf build *.js
