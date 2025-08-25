from openimages.download import download_images

# classes = ["Coffee cup", "Mug"]
classes = [
    "Street light",
    "Jacuzzi", 
    "Drawer", 
    "Dog bed", 
    "Ladder", 
    "Spice rack", 
    "Platter", 
    "Eraser", 
    "Building", 
    "Vase", 
    "Gas stove", 
    "Beaker", 
    "Handbag", 
    "Toilet paper", 
    "Fire hydrant"
]
limit = 100
output_dir = "./train_data/negatives"

download_images(output_dir, classes, exclusions_path=None, limit=limit)
