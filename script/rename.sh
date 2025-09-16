#!/bin/bash
# sadece uzantıyı değiştir (.png -> .tga)

for f in imgs/*.png; do
    if [ -f "$f" ]; then
        base="${f%.png}"
        echo "Renaming $f -> ${base}.tga"
        mv -- "$f" "${base}.tga"
    fi
done
