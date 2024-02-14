# Lyssa

## Overview
Lyssa is a native music player designed to be an aestethic addition for every desktop. 
The player is as easy and [suckless](https://suckless.org/philosophy) as possible to use and work with. The application frontend 
is made to be as modern and aestethic as possible. The goal of this project is to make listening to 
music amazing, visually and audibly. 

The Lyssa application is written in C++ 17. It uses the [Leif Library](https://github.com/cococry/leif), a GUI Library that i've
written in C, for the entire User Interface of the Application. For windowing, the [GLFW Library](https://github.com/glfw/glfw) is used. Additionally, Lyssa depends on [taglib](https://github.com/taglib/taglib) for handling 
ID3 Tags for Music Files. For Audio, Lyssa uses the [miniaudio.h library](https://github.com/mackron/miniaudio).

<div>
    <img src="https://github.com/cococry/lyssa/blob/main/branding/app-on-playlist.png" alt="Alt text" style="height:320px;">
    <img src="https://github.com/cococry/lyssa/blob/main/branding/app-on-track.png" alt="Alt text" style="height:320px;">
</div>

# Dependencies 

#### Dependency List

| Dependency         |  Reason of Usage    |
| ----------------|-------------|
| [leif](https://github.com/cococry/leif) | Creating the entire UI Frontend |
| [taglib](https://github.com/taglib/taglib)| Reading metadata of ID3 tags |
| [miniaudio](https://github.com/mackron/miniaudio) | Audio output of the player | 
| [GLFW](https://github.com/glfw/glfw) | Handling windowing, input etc. | 


As lyssa uses the leif library which also depends on a few things there are four more leif dependecies:
#### Leif Dependencies 

| Dependency         |  Reason of Usage    |
| ----------------|-------------|
| [glad](https://github.com/Dav1dde/glad) | Loading OpenGL functions |
| [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h) | Loading image files into memory |
| [stb_image_resize2](https://github.com/nothings/stb/blob/master/stb_image_resize2.h) | Resizing images |
| [stb_truetype](https://github.com/nothings/stb/blob/master/stb_truetype.h) | Loading font glyphs from font files |
| [cglm](https://github.com/recp/cglm) | Linear Algebra Math | 
| [*GLFW](https://github.com/glfw/glfw) | Handling windowing, input etc. | 

*: This library is an optional library and will be replacable with other libraries


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
- Lyssa is currently only able to build (easily) on linux system at the moment

### Installation

Install dependencies: 

#### Arch (paru): 
```console
sudo paru -S make gcc glfw cglm taglib
```

#### Debian
```console
sudo apt install make gcc libglfw3 libglfw3-dev libcglm-dev libtag1-dev
```
Build the application
```console
make
```

### Running the Application
```console
make run 
```
