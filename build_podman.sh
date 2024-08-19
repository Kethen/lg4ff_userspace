set -e

for arch in i386 amd64 arm64 arm
do
	podman run --rm -it \
		--security-opt label=disable \
		-v ./:/work_dir \
		-w /work_dir \
		--entrypoint /bin/bash \
		--arch $arch \
		debian:bookworm \
		-c '
			set -e
			export DEBIAN_FRONTEND=noninteractive
			apt update
			apt install -y gcc libhidapi-dev
			bash build.sh
		'
	mv lg4ff_userspace lg4ff_userspace_${arch}
done
