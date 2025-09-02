for f in imgs/*.tga; do
    echo "Converting $f..."
    ffmpeg -v quiet -y -i "$f" "${f%.tga}.png" && rm "$f"
done
