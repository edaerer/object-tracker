import openimages.download as oi

# classes = ["Coffee cup", "Mug"]
# classes = ["Street light", "Jacuzzi", "Drawer",  "Dog bed", "Ladder", "Spice rack", "Platter", "Eraser", "Building", "Vase", "Gas stove", "Beaker", "Handbag", "Toilet paper", "Fire hydrant"]
# classes = ["Palm tree", "Juice", "Slow cooker", "Fast food", "Tart", "Cattle", "Footwear", "Alarm clock", "Stool", "Taxi"]
# classes = ["Jellyfish", "Drawer", "Maple", "Book", "Banana", "Scoreboard", "Bed", "Segway", "Boat", "Tortoise", "Bear", "Gondola", "Racket", "Zebra"]
classes = ["Tap", "Dagger", "Shower", "Whiteboard", "Bust", "Pineapple", "Tie"]
limit = 300
output_dir = "../train_data/negatives"

oi.download_images(output_dir, classes, exclusions_path=None, limit=limit)
