playlist_title=$(yt-dlp $1 --flat-playlist --dump-single-json | jq -r .title)
playlist_title_trim=$(echo "$playlist_title" | tr -cd '[:alnum:]')

cd $2
yt-dlp --extract-audio --audio-format mp3 --embed-thumbnail --add-metadata -o "./$playlist_title_trim/%(playlist_index)s - %(title)s.%(ext)s" $1 --download-archive "./$playlist_title_trim/archive.txt"
