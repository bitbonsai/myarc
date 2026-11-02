"""Shadow baking and generated page artwork checks."""
from pathlib import Path
import base64
import io
import re
import unittest

from bs4 import BeautifulSoup
from PIL import Image

from artwork import bake_shadow

ROOT = Path(__file__).resolve().parent


def embedded_image(url):
    mime, encoded = url.split(';base64,', 1)
    assert mime == 'data:image/webp'
    return Image.open(io.BytesIO(base64.b64decode(encoded)))


class ArtworkTests(unittest.TestCase):
    def test_shadow_preserves_artwork_and_adds_transparent_padding(self):
        image = Image.new('RGBA', (16, 16))
        image.paste((230, 30, 20, 255), (4, 4, 12, 12))
        result = bake_shadow(image, radius=2, offset=(0, 3), color=(10, 40, 80, 128), padding=12)
        self.assertEqual((40, 40), result.size)
        self.assertEqual((230, 30, 20, 255), result.getpixel((20, 20)))
        self.assertEqual(0, result.getpixel((0, 0))[3])
        shadow = result.getpixel((20, 29))
        self.assertEqual((10, 40, 80), shadow[:3])
        self.assertGreater(shadow[3], 0)
        self.assertLessEqual(shadow[3], 128)
        self.assertGreater(shadow[3], result.getpixel((20, 10))[3])

    def test_page_uses_baked_webp_and_softer_small_grain(self):
        soup = BeautifulSoup((ROOT / 'index.html').read_text(), 'html.parser')
        for selector, size in [('.hero-art img', 1440), ('.panel-art img', 832), ('.ribbon', 64)]:
            with embedded_image(soup.select_one(selector)['src']) as image:
                self.assertEqual((size, size), image.size)
                self.assertEqual('RGBA', image.mode)
                self.assertEqual(0, image.getpixel((0, 0))[3])
        css = soup.style.string
        grain_url = re.search(r"--grain: url\('([^']+)'\)", css).group(1)
        with embedded_image(grain_url) as image:
            self.assertEqual((256, 256), image.size)
            self.assertEqual((6, 6), image.getchannel('A').getextrema())
        self.assertNotIn('drop-shadow(', css)
        self.assertNotIn('will-change:', css)


if __name__ == '__main__':
    unittest.main()
