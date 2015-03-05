all:
	npm update && \
	./build-all.sh

clean:
	rm -rf build *.js
