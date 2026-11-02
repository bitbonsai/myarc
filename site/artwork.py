"""Bake decorative shadows into transparent artwork before publication."""
from PIL import Image, ImageFilter


def bake_shadow(image, radius, offset, color, padding):
    image = image.convert('RGBA')
    canvas = Image.new('RGBA', (image.width + padding * 2, image.height + padding * 2))
    alpha = Image.new('L', canvas.size)
    alpha.paste(image.getchannel('A'), (padding + offset[0], padding + offset[1]))
    alpha = alpha.filter(ImageFilter.GaussianBlur(radius))
    canvas.paste(color[:3] + (255,), (0, 0, *canvas.size))
    canvas.putalpha(alpha.point(lambda value: round(value * color[3] / 255)))
    canvas.alpha_composite(image, (padding, padding))
    return canvas
