#!/usr/bin/sh
set -e

command_exists() {
  command -v "$@" >/dev/null 2>&1
}

docker_install() {
  sudo tar -zxvf docker-20.10.9.tgz >/dev/null 2>&1
  sudo cp docker/* /usr/bin/
  sudo cp docker.service /etc/systemd/system/
  sudo chmod +x /etc/systemd/system/docker.service
  sudo systemctl daemon-reload
  sudo systemctl enable docker.service
  sudo systemctl start docker
}

do_install() {
  if command_exists docker; then
    cat >&2 <<-'EOF'
			Warning: the "docker" command appears to already exist on this system
			abort this script
		EOF
    exit 1
  fi

  docker_install

  if command_exists docker; then
    (
      set +x
      docker load -i centos7.tar >/dev/null 2>&1
    )
    cat >&2 <<-'EOF'
			docker install success
		EOF
    exit 0
  fi

}

do_install
