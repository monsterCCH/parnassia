#!/bin/sh
set -e
sudo systemctl stop docker
sudo rm -f /etc/systemd/system/docker.service
sudo rm -rf /usr/bin/docker*
sudo systemctl daemon-reload
sudo rm -rf /etc/parnassia.cfg
echo 'uninstall success'
