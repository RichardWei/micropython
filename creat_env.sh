#!/bin/bash


sudo apt-get update
sudo apt-get install -y build-essential git gcc-arm-none-eabi 

git submodule update --init --recursive


git remote add upstream https://github.com/micropython/micropython.git
git remote -v
git fetch upstream