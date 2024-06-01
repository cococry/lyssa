yt-dlp --compat-options no-youtube-unavailable-videos --flat-playlist -j --no-warnings $1 | jq -c .n_entries | head -n 1
