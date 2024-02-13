# Lyssa


## Overview

Lyssa is a native music player designed to be an aestethic addition for every desktop. 
It is easy to use, minimalistic and modern. The goal of this project is to make listening to 
music amazing, visually and audibly. 

The Lyssa application is written in C++ 17. It uses the [Leif Library](https://github.com/cococry/leif), a GUI Library that i've
written in C, for the entire User Interface of the Application. For windowing, the [GLFW Library](https://github.com/glfw/glfw) is used. Additionally, Lyssa depends on [taglib](https://github.com/taglib/taglib) for handling 
ID3 Tags for Music Files. For Audio, Lyssa uses the [miniaudio.h library](https://github.com/mackron/miniaudio).

#### Dependency List
- [leif](https://github.com/cococry/leif)
- [glfw](https://github.com/glfw/glfw)
- [taglib](https://github.com/taglib/taglib)
- [miniaudio](https://github.com/mackron/miniaudio)

As lyssa uses the leif library which also depends on a few things there are four more leif dependecies:
#### Leif Dependencies 
- [(GLFW)](https://github.com/glfw/glfw)
- [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h)
- [(stb_image_resize)](https://github.com/nothings/stb/blob/master/stb_image_resize2.h)
- [stb_truetype](https://github.com/nothings/stb/blob/master/stb_truetype.h)
- [glad](https://github.com/Dav1dde/glad)

<img src="https://github.com/cococry/lyssa/blob/main/branding/app-on-playlist.png"  width ="550px"/> 
<img src="https://github.com/cococry/lyssa/blob/main/branding/app-on-track.png"  width="400px"/> 

## Features

- [x] Creating multiple playlists
- [x] Playing mp3 music files
- [x] Playlist controls
- [x] Adjusting UI for different window resoultions
- [x] Deleting Playlists
- [x] Deleting files in Playlist
- [x] Loading files into playlists dynamically
- [ ] Using software like yt-dlp to download YouTube playlist UI 
- [ ] Playlist Shuffle Mode
- [ ] Searching Playlists for Files
- [ ] Moving files in playlists

## Build Instructions

### Notes 
- The building process will be reworked entirely in the future
- Lyssa is currently only able to build (easily) on linux system at the moment

### Installation

Clone the repository:
```console
git clone https://github.com/cococry/lyssa
```

Install dependencies: 

#### Arch: 
```console
sudo pacman -S glfw-x11 cglm taglib
```

#### Debian
```console
sudo apt install libglfw3 libglfw3-dev libcglm-dev libtag1-dev
```

Run the install script:
```console
cd lyssa/scripts
./install.sh
```

### Running the Application

Run the build_run script
```console
cd lyssa/scripts
./build_run.sh
```

Or run the run_game script
```console
cd lyssa/scripts
./run_game
```
