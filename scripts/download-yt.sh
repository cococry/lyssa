playlist_title=$(yt-dlp $1 --flat-playlist --dump-single-json | jq -r .title)

cd $2
yt-dlp -o "./%(playlist_title)s/%(title)s.%(ext)s" --extract-audio --audio-format mp3 --add-metadata --embed-thumbnail --download-archive "./$playlist_title/archive.txt" $1 
