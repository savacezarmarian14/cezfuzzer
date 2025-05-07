#!/bin/bash

# This script installs the necessary dependencies for building the project.
# It is designed to be run on Ubuntu 20.04 or later.
# Ensure the script is run with superuser privileges

# Bulding the project requires the following dependencies:
sudo apt-get update
sudo apt-get -y install build-essential
sudo apt-get -y install cmake

# Docker is required for building the project
sudo apt-get -y install ca-certificates curl
sudo install -m 0755 -d /etc/apt/keyrings
sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc

echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu \
  $(. /etc/os-release && echo "${UBUNTU_CODENAME:-$VERSION_CODENAME}") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt-get update

sudo apt-get -y install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

# lib yaml-cpp
sudo apt install libyaml-cpp-dev
