"""Makes the test images for test_image.cpp, from plain shapes rather than box art, since BGG's
thumbnails are publishers' art. Needs Pillow; run it from this folder and commit what it writes.
"""
from PIL import Image, ImageDraw

# 200 x 150, the shape of a BGG thumbnail, progressive like BGG's JPEGs: the left third black,
# the middle third mid-grey, the right third white.
image = Image.new("L", (200, 150), 255)
draw = ImageDraw.Draw(image)
draw.rectangle([0, 0, 65, 149], fill=0)
draw.rectangle([66, 0, 132, 149], fill=128)
image.convert("RGB").save("image_progressive.jpg", "JPEG", quality=90, progressive=True)

# The same, baseline.
image.convert("RGB").save("image_baseline.jpg", "JPEG", quality=90, progressive=False)

# 100 x 100 with alpha: the top half transparent (and black underneath, which mustn't show),
# the bottom half opaque black.
rgba = Image.new("RGBA", (100, 100), (0, 0, 0, 0))
ImageDraw.Draw(rgba).rectangle([0, 50, 99, 99], fill=(0, 0, 0, 255))
rgba.save("image_alpha.png", "PNG")

# 1000 x 1000, more pixels than the app accepts.
Image.new("L", (1000, 1000), 200).save("image_large.png", "PNG")
