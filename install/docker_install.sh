#!/bin/sh

docker --version
if [ $? -eq 0 ]; then
  echo "docker already install"
else
  sudo tar -zxvf docker-20.10.9.tgz
  sudo cp docker/* /usr/bin/
  sudo cp docker.service /etc/systemd/system/
  sudo chmod +x /etc/systemd/system/docker.service

  systemctl daemon-reload
  systemctl enable docker.service
  systemctl start docker
fi
