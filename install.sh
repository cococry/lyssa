#!/bin/bash

install_with_apt() {
    sudo apt update
    sudo apt install -y ffmpeg exiftool jq libglfw3 libglfw3-dev
    sudo apt install -y python3-pip
    pip3 install --upgrade yt-dlp
}

install_with_yum() {
    sudo yum install -y epel-release
    sudo yum install -y ffmpeg perl-Image-ExifTool jq glfw glfw-devel
    sudo yum install -y python3-pip
    pip3 install --upgrade yt-dlp
}

install_with_pacman() {
    sudo pacman -Sy --noconfirm ffmpeg exiftool jq glfw
    sudo pacman -S --noconfirm python-pip
    pip install --upgrade yt-dlp
}

if [ -f /etc/arch-release ]; then
  install_with_pacman
elif [ -f /etc/debian_version ]; then
  install_with_apt
elif [ -f /etc/redhat-release ] || [ -f /etc/centos-release ]; then
  install_with_yum
else
  echo "Your linux distro is not supported currently."
  echo "You need to manually install those packages: exiftool, jq, glfw"
fi


echo -e "\e[32mBuilding additional depdencies from source\e[0m"

# libleif 
echo -e "\e[32mBuilding libleif\e[0m"
git clone https://github.com/cococry/leif
cd leif
./install.sh
cd ..
rm -rf leif

# taglib
echo -e "\e[32mBuilding taglib\e[0m"
git clone --recursive https://github.com/taglib/taglib.git
cd taglib
mkdir build
cd build
cmake ..
make
sudo make install
cd ../..
rm -rf taglib

make && sudo make install
if [ $? -eq 0 ]; then
  echo -e "\e[32mBuilt lyssa successfully\e[0m"
  echo "Copied .desktop file into /usr/share/applications."
  echo "Copied binaries into /usr/bin"
fi 

read -p "Do you want to start lyssa (y/n): " answer

# Convert the answer to lowercase to handle Y/y and N/n
answer=${answer,,}

# Check the user's response
if [[ "$answer" == "y" ]]; then
    echo "Starting..."
    lyssa
elif [[ "$answer" == "n" ]]; then
  echo "Not starting the app."
else
    echo "Invalid input. Please enter y or n."
fi
