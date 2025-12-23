#!/bin/bash


sudo apt-get update
sudo apt-get install -y build-essential git gcc-arm-none-eabi 

git submodule update --init --recursive


