all:
	docker build --network=host -f Dockerfile -t build-in-ubuntu-2204 .
	rm -rf output && mkdir -p output
	docker run --rm -it -v $$PWD/output:/home/build/output build-in-ubuntu-2204 cp -r podman_image/tmpWorkSpace /home/build/output/
	docker build -f ../podman_image/Containerfile.misrac100k4 -t naive.systems/analyzer/misra:dev_en ./output/tmpWorkSpace
