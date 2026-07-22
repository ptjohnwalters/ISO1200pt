#!/usr/bin/env python3
"""
generate_iop.py
Converts PoolEdit XML object pool to binary .iop format
For Case IH 1200PT Custom ECU Project
"""

import xml.etree.ElementTree as ET
import struct
import sys
import os

# ─── ISOBUS VT Object Type IDs ───────────────────────────────────────────────
OBJECT_TYPES = {
    'workingset':       0,
    'datamask':         1,
    'alarmmask':        2,
    'container':        3,
    'softkeymask':      4,
    'key':              5,
    'button':           6,
    'inputboolean':     7,
    'inputstring':      8,
    'inputnumber':      9,
    'inputlist':        10,
    'outputstring':     11,
    'outputnumber':     12,
    'outputlist':       13,
    'outputline':       14,
    'outputrectangle':  15,
    'outputellipse':    16,
    'outputpolygon':    17,
    'outputmeter':      18,
    'outputlinearbargraph': 19,
    'outputarchedbargraph': 20,
    'picturegraphic':   21,
    'numbervar':        22,
    'stringvar':        23,
    'fontattributes':   24,
    'lineattributes':   25,
    'fillattributes':   26,
    'inputattributes':  27,
    'objectpointer':    28,
    'macro':            29,
    'auxiliaryfunction': 30,
    'auxiliaryinput':   31,
}

# ─── COLOUR MAP ──────────────────────────────────────────────────────────────
COLOURS = {
    'black':        0,
    'white':        1,
    'green':        2,
    'teal':         3,
    'maroon':       4,
    'purple':       5,
    'olive':        6,
    'silver':       7,
    'grey':         8,
    'blue':         9,
    'lime':         10,
    'cyan':         11,
    'red':          12,
    'magenta':      13,
    'yellow':       14,
    'navy':         15,
}

# ─── FONT SIZE MAP ───────────────────────────────────────────────────────────
FONT_SIZES = {
    '6x8':      0,
    '8x8':      1,
    '8x12':     2,
    '12x16':    3,
    '16x16':    4,
    '16x24':    5,
    '24x32':    6,
    '32x32':    7,
    '32x48':    8,
    '48x64':    9,
    '64x64':    10,
    '64x96':    11,
    '96x128':   12,
    '128x128':  13,
    '128x192':  14,
}

JUSTIFICATION_H = {
    'left':     0,
    'centred':  1,
    'right':    2,
    'middle':   1,
}

JUSTIFICATION_V = {
    'top':      0,
    'middle':   1,
    'bottom':   2,
}

def get_colour(name):
    return COLOURS.get(str(name).lower(), 0)

def get_id(obj_map, name):
    if name in obj_map:
        return obj_map[name]
    return 0xFFFF

def parse_id(element):
    id_str = element.get('id', '0')
    try:
        return int(id_str)
    except ValueError:
        return 0

def encode_string(s, length):
    encoded = s.encode('latin-1', errors='replace')
    if len(encoded) < length:
        encoded += b'\x00' * (length - len(encoded))
    return encoded[:length]

class IOPGenerator:
    def __init__(self, xml_file):
        self.xml_file = xml_file
        self.objects = {}
        self.output = bytearray()
        self.name_to_id = {}

    def load_xml(self):
        tree = ET.parse(self.xml_file)
        self.root = tree.getroot()
        # First pass - build name to ID map
        for elem in self.root.iter():
            name = elem.get('name')
            id_val = elem.get('id')
            if name and id_val:
                try:
                    self.name_to_id[name] = int(id_val)
                except ValueError:
                    pass

    def get_children(self, element):
        return [c for c in element if c.tag == 'include_object']

    def resolve_id(self, name):
        return self.name_to_id.get(name, 0xFFFF)

    def write_uint8(self, val):
        self.output += struct.pack('B', int(val) & 0xFF)

    def write_uint16(self, val):
        self.output += struct.pack('<H', int(val) & 0xFFFF)

    def write_uint32(self, val):
        self.output += struct.pack('<I', int(val) & 0xFFFFFFFF)

    def encode_workingset(self, elem):
        obj_id = parse_id(elem)
        children = self.get_children(elem)
        bg_colour = get_colour(elem.get('background_colour', 'black'))
        selectable = 1 if elem.get('selectable', 'yes') == 'yes' else 0

        # Find active mask
        active_mask = 0xFFFF
        for child in children:
            role = child.get('role', '')
            if role == 'active_mask':
                active_mask = self.resolve_id(child.get('name', ''))

        self.write_uint16(obj_id)
        self.write_uint8(OBJECT_TYPES['workingset'])
        self.write_uint8(bg_colour)
        self.write_uint16(active_mask)
        self.write_uint8(0)  # object count
        self.write_uint8(0)  # macro count
        self.write_uint8(0)  # language count
        print(f"  WorkingSet ID={obj_id} active_mask={active_mask}")

    def encode_datamask(self, elem):
        obj_id = parse_id(elem)
        children = self.get_children(elem)
        bg_colour = get_colour(elem.get('background_colour', 'black'))

        soft_key_mask = 0xFFFF
        object_refs = []

        for child in children:
            role = child.get('role', '')
            name = child.get('name', '')
            if role == 'soft_key_mask':
                soft_key_mask = self.resolve_id(name)
            else:
                ref_id = self.resolve_id(name)
                if ref_id != 0xFFFF:
                    object_refs.append(ref_id)

        self.write_uint16(obj_id)
        self.write_uint8(OBJECT_TYPES['datamask'])
        self.write_uint8(bg_colour)
        self.write_uint16(soft_key_mask)
        self.write_uint8(len(object_refs))
        self.write_uint8(0)  # macro count
        for ref in object_refs:
            self.write_uint16(ref)
            self.write_uint16(0)  # x
            self.write_uint16(0)  # y
        print(f"  DataMask ID={obj_id} skm={soft_key_mask} children={len(object_refs)}")

    def encode_softkeymask(self, elem):
        obj_id = parse_id(elem)
        children = self.get_children(elem)
        bg_colour = get_colour(elem.get('background_colour', 'black'))

        key_refs = []
        for child in children:
            name = child.get('name', '')
            ref_id = self.resolve_id(name)
            if ref_id != 0xFFFF:
                key_refs.append(ref_id)

        self.write_uint16(obj_id)
        self.write_uint8(OBJECT_TYPES['softkeymask'])
        self.write_uint8(bg_colour)
        self.write_uint8(len(key_refs))
        self.write_uint8(0)  # macro count
        for ref in key_refs:
            self.write_uint16(ref)
        print(f"  SoftKeyMask ID={obj_id} keys={len(key_refs)}")

    def encode_key(self, elem):
        obj_id = parse_id(elem)
        children = self.get_children(elem)
        bg_colour = get_colour(elem.get('background_colour', 'black'))
        key_code = int(elem.get('key_code', '0'))

        object_refs = []
        for child in children:
            name = child.get('name', '')
            ref_id = self.resolve_id(name)
            if ref_id != 0xFFFF:
                object_refs.append(ref_id)

        self.write_uint16(obj_id)
        self.write_uint8(OBJECT_TYPES['key'])
        self.write_uint8(bg_colour)
        self.write_uint8(key_code)
        self.write_uint8(len(object_refs))
        self.write_uint8(0)  # macro count
        for ref in object_refs:
            self.write_uint16(ref)
            self.write_uint16(0)  # x
            self.write_uint16(0)  # y
        print(f"  Key ID={obj_id} key_code={key_code} children={len(object_refs)}")

    def encode_button(self, elem):
        obj_id = parse_id(elem)
        children = self.get_children(elem)
        width = int(elem.get('width', '100'))
        height = int(elem.get('height', '50'))
        bg_colour = get_colour(elem.get('background_colour', 'black'))
        border_colour = get_colour(elem.get('border_colour', 'white'))
        key_code = int(elem.get('key_code', '0'))

        object_refs = []
        for child in children:
            name = child.get('name', '')
            ref_id = self.resolve_id(name)
            if ref_id != 0xFFFF:
                object_refs.append(ref_id)

        self.write_uint16(obj_id)
        self.write_uint8(OBJECT_TYPES['button'])
        self.write_uint16(width)
        self.write_uint16(height)
        self.write_uint8(bg_colour)
        self.write_uint8(border_colour)
        self.write_uint8(key_code)
        self.write_uint8(0)  # options
        self.write_uint8(len(object_refs))
        self.write_uint8(0)  # macro count
        for ref in object_refs:
            self.write_uint16(ref)
            self.write_uint16(0)  # x
            self.write_uint16(0)  # y
        print(f"  Button ID={obj_id} {width}x{height} children={len(object_refs)}")

    def encode_outputstring(self, elem):
        obj_id = parse_id(elem)
        children = self.get_children(elem)
        width = int(elem.get('width', '100'))
        height = int(elem.get('height', '20'))
        font_attr = 0xFFFF
        bg_colour = get_colour(elem.get('background_colour', 'black'))
        value = elem.get('value', '')
        length = int(elem.get('length', str(len(value) + 1)))
        h_just = JUSTIFICATION_H.get(elem.get('horizontal_justification', 'left'), 0)
        v_just = JUSTIFICATION_V.get(elem.get('vertical_justification', 'top'), 0)
        options = (v_just << 4) | h_just

        for child in children:
            role = child.get('role', '')
            if role == 'font_attributes':
                font_attr = self.resolve_id(child.get('name', ''))

        self.write_uint16(obj_id)
        self.write_uint8(OBJECT_TYPES['outputstring'])
        self.write_uint16(width)
        self.write_uint16(height)
        self.write_uint16(font_attr)
        self.write_uint8(options)
        self.write_uint16(0xFFFF)  # variable reference
        self.write_uint8(h_just)
        encoded = encode_string(value, length)
        self.write_uint8(length)
        self.output += encoded
        self.write_uint8(0)  # macro count
        print(f"  OutputString ID={obj_id} '{value[:20]}'")

    def encode_outputnumber(self, elem):
        obj_id = parse_id(elem)
        children = self.get_children(elem)
        width = int(elem.get('width', '100'))
        height = int(elem.get('height', '30'))
        font_attr = 0xFFFF
        bg_colour = get_colour(elem.get('background_colour', 'black'))
        value = int(float(elem.get('value', '0')))
        offset = int(float(elem.get('offset', '0')))
        scale = float(elem.get('scale', '1.0'))
        h_just = JUSTIFICATION_H.get(
            elem.get('horizontal_justification', 'left'), 0)

        for child in children:
            role = child.get('role', '')
            if role == 'font_attributes':
                font_attr = self.resolve_id(child.get('name', ''))

        self.write_uint16(obj_id)
        self.write_uint8(OBJECT_TYPES['outputnumber'])
        self.write_uint16(width)
        self.write_uint16(height)
        self.write_uint16(font_attr)
        self.write_uint8(0)  # options
        self.write_uint16(0xFFFF)  # variable reference
        self.write_uint8(h_just)
        self.write_uint32(value)
        self.write_uint32(offset)
        # Scale as float32
        self.output += struct.pack('<f', scale)
        self.write_uint8(0)  # number of decimals
        self.write_uint8(0)  # format (fixed)
        self.write_uint8(0)  # macro count
        print(f"  OutputNumber ID={obj_id} value={value}")

    def encode_fontattributes(self, elem):
        obj_id = parse_id(elem)
        colour = get_colour(elem.get('font_colour', 'white'))
        size_str = elem.get('font_size', '8x8')
        size = FONT_SIZES.get(size_str, 1)
        style = 0  # normal
        font_type = 0  # latin1

        self.write_uint16(obj_id)
        self.write_uint8(OBJECT_TYPES['fontattributes'])
        self.write_uint8(colour)
        self.write_uint8(size)
        self.write_uint8(font_type)
        self.write_uint8(style)
        self.write_uint8(0)  # macro count
        print(f"  FontAttributes ID={obj_id} size={size_str} colour={colour}")

    def generate(self, output_file):
        print(f"Loading XML: {self.xml_file}")
        self.load_xml()
        print(f"Found {len(self.name_to_id)} named objects")
        print(f"Generating binary .iop: {output_file}")

        # Process objects in correct order
        # WorkingSet must be first
        encoders = {
            'workingset':       self.encode_workingset,
            'softkeymask':      self.encode_softkeymask,
            'key':              self.encode_key,
            'datamask':         self.encode_datamask,
            'button':           self.encode_button,
            'outputstring':     self.encode_outputstring,
            'outputnumber':     self.encode_outputnumber,
            'fontattributes':   self.encode_fontattributes,
        }

        # First encode workingset
        for elem in self.root.iter('workingset'):
            self.encode_workingset(elem)

        # Then font attributes (referenced by everything)
        for elem in self.root.iter('fontattributes'):
            self.encode_fontattributes(elem)

        # Then soft key masks and keys
        for elem in self.root.iter('softkeymask'):
            self.encode_softkeymask(elem)
        for elem in self.root.iter('key'):
            self.encode_key(elem)

        # Then buttons and strings
        for elem in self.root.iter('button'):
            self.encode_button(elem)
        for elem in self.root.iter('outputstring'):
            self.encode_outputstring(elem)
        for elem in self.root.iter('outputnumber'):
            self.encode_outputnumber(elem)

        # Data masks last
        for elem in self.root.iter('datamask'):
            self.encode_datamask(elem)

        # Write output file
        with open(output_file, 'wb') as f:
            f.write(self.output)

        print(f"\nSuccess! Generated {len(self.output)} bytes")
        print(f"Output: {output_file}")

if __name__ == '__main__':
    # Paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    xml_input  = os.path.join(script_dir, 'ISO1200PT')
    iop_output = os.path.join(
        script_dir,
        'examples', '1200PT', 'src', 'object_pool', 'object_pool.iop'
    )

    if not os.path.exists(xml_input):
        print(f"ERROR: XML file not found: {xml_input}")
        print("Make sure ISO1200PT.xml is in the same folder as this script")
        sys.exit(1)

    os.makedirs(os.path.dirname(iop_output), exist_ok=True)

    generator = IOPGenerator(xml_input)
    generator.generate(iop_output)