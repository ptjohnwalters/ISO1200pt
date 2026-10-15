#!/usr/bin/env python3
"""Render documentary 480x480 PNG previews of the 1200PT VT data masks.

The previews are generated from the ``object_pool`` XML (rectangles and text
only) and are not part of the VT object pool.  Pillow is required for
rendering; the layout helpers below only need the standard library.

Usage: python3 scripts/render_vt_preview.py [output_dir]
"""

import os
import sys
import xml.etree.ElementTree as ET

REPOSITORY_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
POOL_XML = os.path.join(REPOSITORY_ROOT, 'object_pool')
DEFAULT_OUTPUT = os.path.join(REPOSITORY_ROOT, 'docs', 'vt_preview')

MASKS = {
    'run': 'DataMask_Run',
    'unfold': 'DataMask_Unfold',
    'fold': 'DataMask_Fold',
    'cal': 'DataMask_Cal',
}

COLOURS = {
    'black': (0, 0, 0), 'white': (255, 255, 255), 'green': (0, 153, 0),
    'teal': (0, 153, 153), 'maroon': (153, 0, 0), 'purple': (153, 0, 153),
    'olive': (153, 153, 0), 'silver': (204, 204, 204), 'grey': (153, 153, 153),
    'blue': (0, 0, 255), 'lime': (0, 255, 0), 'cyan': (0, 255, 255),
    'red': (255, 0, 0), 'magenta': (255, 0, 255), 'yellow': (255, 255, 0),
    'navy': (0, 0, 153),
}


def load_pool(path=POOL_XML):
    """Return (objects by name, fontattributes by name, datamask elements by name)."""
    root = ET.parse(path).getroot()
    objects = {e.get('name'): e for e in root.iter()
               if e.tag not in ('objectpool', 'include_object') and e.get('name')}
    return objects, {n: e for n, e in objects.items() if e.tag == 'fontattributes'}


def font_cell(font):
    """Return (char_width, char_height) of a fontattributes element."""
    width, height = font.get('font_size').split('x')
    return int(width), int(height)


def rect(element):
    return (int(element.get('x', 0)), int(element.get('y', 0)),
            int(element.get('width', 0)), int(element.get('height', 0)))


def mask_items(objects, mask_name):
    """Return [(element, absolute_rect)] for the top-level objects of a data mask."""
    return [(objects[c.get('name')], rect(objects[c.get('name')]))
            for c in objects[mask_name] if c.tag == 'include_object']


def button_children(objects, button):
    """Return [(element, absolute_rect)] for the labels included in a button."""
    bx, by, _, _ = rect(button)
    result = []
    for child in button:
        if child.tag != 'include_object':
            continue
        element = objects[child.get('name')]
        cx, cy, cw, ch = rect(element)
        result.append((element, (bx + cx, by + cy, cw, ch)))
    return result


def _truetype(size):
    from PIL import ImageFont
    for candidate in ('DejaVuSansMono.ttf', 'LiberationMono-Regular.ttf'):
        try:
            return ImageFont.truetype(candidate, size)
        except OSError:
            continue
    return ImageFont.load_default()


def _draw_text(draw, element, box, font_element):
    char_w, char_h = font_cell(font_element)
    text = element.get('value', '')
    if element.tag == 'outputnumber':
        text = str(int(element.get('value', 0)))
    if not text.strip():
        return
    x, y, w, h = box
    font = _truetype(char_h)
    text_w = len(text) * char_w
    hj = element.get('horizontal_justification', 'left')
    vj = element.get('vertical_justification', 'top')
    tx = x + ((w - text_w) // 2 if hj in ('centred', 'centered', 'middle') else
              (w - text_w if hj == 'right' else 0))
    ty = y + ((h - char_h) // 2 if vj in ('middle', 'center', 'centre') else
              (h - char_h if vj == 'bottom' else 0))
    for index, char in enumerate(text):
        draw.text((tx + index * char_w, ty), char, font=font,
                  fill=COLOURS[font_element.get('font_colour')])


def render_mask(objects, fonts, mask_name):
    from PIL import Image, ImageDraw
    image = Image.new('RGB', (480, 480), COLOURS[objects[mask_name].get('background_colour')])
    draw = ImageDraw.Draw(image)

    def draw_output(element, box):
        x, y, w, h = box
        draw.rectangle([x, y, x + w - 1, y + h - 1], fill=COLOURS[element.get('background_colour')])
        _draw_text(draw, element, box, fonts[element.get('font_attributes')])

    for element, box in mask_items(objects, mask_name):
        if element.tag == 'button':
            x, y, w, h = box
            draw.rectangle([x, y, x + w - 1, y + h - 1],
                           fill=COLOURS[element.get('background_colour')],
                           outline=COLOURS[element.get('border_colour')])
            for child, child_box in button_children(objects, element):
                _draw_text(draw, child, child_box, fonts[child.get('font_attributes')])
        else:
            draw_output(element, box)
    return image


def main(argv):
    output_dir = argv[1] if len(argv) > 1 else DEFAULT_OUTPUT
    os.makedirs(output_dir, exist_ok=True)
    objects, fonts = load_pool()
    for key, mask_name in MASKS.items():
        path = os.path.join(output_dir, f'{key}.png')
        render_mask(objects, fonts, mask_name).save(path)
        print(f'Wrote {path}')


if __name__ == '__main__':
    main(sys.argv)
