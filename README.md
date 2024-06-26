# Lyssa

## Overview
Lyssa is a native music player designed to be an aestethic addition for every desktop. 
The player is as easy and [suckless](https://suckless.org/philosophy) as possible to use and to work with. The application frontend 
is made to be modern and aestethic. The goal of this project is to make listening to 
music amazing, visually and audibly. 

Lyssa has the capability to easily download music from 
various streaming services. Under the hood, yt-dlp 
is used to download the music files. 

The Lyssa application is written in C++ 17. It uses the [Leif Library](https://github.com/cococry/leif), a GUI Library that i've
written in C, for the entire User Interface of the Application. For windowing, the [GLFW Library](https://github.com/glfw/glfw) is used. Additionally, Lyssa depends on [taglib](https://github.com/taglib/taglib) for handling 
ID3 Tags for Music Files. For Audio, Lyssa uses the [miniaudio.h library](https://github.com/mackron/miniaudio).

<img src="https://github.com/cococry/lyssa/blob/main/branding/lyssa-showcase.png" alt="Lyssa Showcase">


# Dependencies 

#### Build Dependecies

| Dependency         |  Reason of Usage    |
| ----------------|-------------|
| [leif](https://github.com/cococry/leif) | Creating the entire UI Frontend |
| [taglib](https://github.com/taglib/taglib)| Reading metadata of ID3 tags |
| [miniaudio](https://github.com/mackron/miniaudio) | Audio output of the player | 
| [GLFW](https://github.com/glfw/glfw) | Handling windowing, input etc. | 

#### Runtime Dependencies

| Dependency         |  Reason of Usage    |
| ----------------|-------------|
| [yt-dlp](https://github.com/yt-dlp/yt-dlp) | Downloading playlists |
| [jq](https://github.com/jqlang/jq) | Parsing JSON of playlists |
| [ffmpeg](https://github.com/FFmpeg/FFmpeg)| yt-dlp needs ffmpeg for extracting images |
| [exiftool](https://exiftool.org/)| Retrieving metadata of sound files |


As lyssa uses the leif library which also depends on a few things there are some more leif dependecies:
#### Leif Dependencies 

| Dependency         |  Reason of Usage    |
| ----------------|-------------|
| [glad](https://github.com/Dav1dde/glad) | Loading OpenGL functions |
| [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h) | Loading image files into memory |
| [stb_image_resize2](https://github.com/nothings/stb/blob/master/stb_image_resize2.h) | Resizing images |
| [stb_truetype](https://github.com/nothings/stb/blob/master/stb_truetype.h) | Loading font glyphs from font files |
| [cglm](https://github.com/recp/cglm) | Linear Algebra Math | 
| [GLFW](https://github.com/glfw/glfw) | Handling windowing, input etc. | 
| [libclipboard](https://github.com/jtanx/libclipboard) | Copy + Paste for input fields |


## Features

- [x] Creating multiple playlists
- [x] Playing mp3 music files
- [x] Playlist controls
- [x] Adjusting UI for different window resoultions
- [x] Deleting Playlists
- [x] Deleting files in Playlist
- [x] Loading files into playlists dynamically
- [x] Downloading playlists from streaming services 
- [x] Playlist Shuffle Mode
- [x] Replay mode for tracks
- [x] Fullscreened Tracks
- [x] Searching Playlists for Files
- [x] Moving files in playlists

## Build Instructions

### Notes 
- Lyssa is currently only able to build (easily) on linux system at the moment

### Installation
```console
git clone https://github.com/cococry/lyssa
cd lyssa
./install.sh
```

This will install & build all the depdencies before installing
lyssa onto your system.

Uninstalling
```
sudo make uninstall
```

### Running the Application
```console
lyssa 
```

## Contributing
You can contribute to Lyssa by:
  - [Fixing bugs or contributing to features](https://github.com/cococry/lyssa/issues)
  - [Changing features or adding new functionality](https://github.com/cococry/lyssa/pulls)
