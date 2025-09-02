for f in imgs/*.png; do
    echo "Flipping $f..."
    ffmpeg -y -i "$f" -vf vflip "$f"
done
