playlist_title=$(yt-dlp https://www.youtube.com/playlist\?list\=PLBoHUDtT-LknhaEoiuMDPvAGHUGQepTcB --flat-playlist --dump-single-json | jq -r .title)
yt-dlp -o "~/%(playlist_title)s/%(title)s.%(ext)s" --extract-audio --audio-format mp3 --add-metadata --embed-thumbnail --download-archive "~/$playlist_title/archive.txt" $1 
