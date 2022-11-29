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

kvm_install() {
   if command_exists yum; then
       sudo tar -zxvf repository.tar.gz -C /root >/dev/null 2>&1
       sudo cp local-custom.repo /etc/yum.repos.d/
       sudo yum -y --disablerepo=\* --enablerepo=local-custom install qemu-kvm python-virtinst libvirt libvirt-python virt-manager libguestfs-tools bridge-utils virt-install >/dev/null 2>&1
   fi
}

do_install() {
  if command_exists docker; then
	echo "Warning: the 'docker' command appears to already exist on this system"
  else
    docker_install
    (
      set +x
      docker load -i centos_7_bigtree.tar >/dev/null 2>&1
    )
    echo "docker install success"
  fi

if command_exists virt-install; then
    echo "Warning: the 'kvm' command appears to already exist on this system"
else
  kvm_install
  systemctl start libvirtd
  systemctl enable libvirtd
  echo "kvm install success"
fi


}

do_install
