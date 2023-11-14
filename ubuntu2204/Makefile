all:
	podman build -f Containerfile -t build-in-ubuntu-2204

enter:
	mkdir -p podman_image
	podman run --rm -it --userns=keep-id:uid=1000,gid=1000 -v $$PWD/podman_image:/home/build/podman_image build-in-ubuntu-2204 /bin/bash
