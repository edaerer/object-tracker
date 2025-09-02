#!/usr/bin/env bash
set -euo pipefail

# --- AYARLAR ---
SRC_DIR="imgs"      # .img dosyalarının olduğu yer
DEST_DIR="images"    # .img dosyalarının taşınacağı yer
CLEAN_DIR="$SRC_DIR"       # Boş .txt ve ilişkili görsellerin temizleneceği yer
IMG_EXTS=(png jpg jpeg tga bmp webp)  # txt ile aynı isimli görsel uzantıları

# 1) .img dosyalarını DEST_DIR'e taşı
mkdir -p "$DEST_DIR"
find "$SRC_DIR" -type f -name '*.img' -print0 \
| while IFS= read -r -d '' f; do
  echo "Taşı: $f -> $DEST_DIR/"
  mv -v "$f" "$DEST_DIR/"
done

# 2) Boş .txt dosyalarını ve bağlı görselleri sil
find "$CLEAN_DIR" -type f -name '*.txt' -size 0 -print0 \
| while IFS= read -r -d '' txt; do
  base="${txt%.*}"
  echo "Sil: $txt"
  rm -v -- "$txt"
  for ext in "${IMG_EXTS[@]}"; do
    img="${base}.${ext}"
    if [[ -f "$img" ]]; then
      echo "Bağlı görseli sil: $img"
      rm -v -- "$img"
    fi
  done
done
