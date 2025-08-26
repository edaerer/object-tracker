import os
import cv2

IMG_DIR = "dataset/images"
LABEL_DIR = "dataset/labels"
OUT_POS_DIR = "dataset/positives"
OUT_NEG_DIR = "dataset/negatives"

os.makedirs(OUT_POS_DIR, exist_ok=True)
os.makedirs(OUT_NEG_DIR, exist_ok=True)

for fname in os.listdir(IMG_DIR):
    if not fname.endswith(('.jpg', '.png')):
        continue

    img_path = os.path.join(IMG_DIR, fname)
    label_path = os.path.join(LABEL_DIR, os.path.splitext(fname)[0] + '.txt')

    if not os.path.exists(label_path):
        print(f"Label bulunamadı: {label_path}")
        continue

    img = cv2.imread(img_path)
    if img is None:
        print(f"Resim okunamadı: {img_path}")
        continue

    h, w, _ = img.shape

    with open(label_path, 'r') as f:
        for idx, line in enumerate(f):
            parts = line.strip().split()
            if len(parts) != 5:
                continue

            _, x_center, y_center, bw, bh = map(float, parts)

            # Oransal koordinatları piksele çevir
            cx = int(x_center * w)
            cy = int(y_center * h)
            bw = int(bw * w)
            bh = int(bh * h)

            x1 = max(cx - bw // 2, 0)
            y1 = max(cy - bh // 2, 0)
            x2 = min(cx + bw // 2, w)
            y2 = min(cy + bh // 2, h)

            crop = img[y1:y2, x1:x2]
            gray = cv2.cvtColor(crop, cv2.COLOR_BGR2GRAY)

            out_path = os.path.join(OUT_POS_DIR, f"{os.path.splitext(fname)[0]}_{idx}.png")
            cv2.imwrite(out_path, gray)

print("✅ Kırpma tamamlandı.")
