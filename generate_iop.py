#!/usr/bin/env python3
"""
generate_iop.py
Converts PoolEdit XML object pool to binary .iop format.
"""

import os
import struct
import sys
import xml.etree.ElementTree as ET

# ISOBUS VT Object Type IDs used by the repository VT parser.
OBJECT_TYPES = {
    'workingset': 0,
    'datamask': 1,
    'alarmmask': 2,
    'container': 3,
    'softkeymask': 4,
    'key': 5,
    'button': 6,
    'inputboolean': 7,
    'inputstring': 8,
    'inputnumber': 9,
    'inputlist': 10,
    'outputstring': 11,
    'outputnumber': 12,
    'outputline': 13,
    'outputrectangle': 14,
    'outputellipse': 15,
    'outputpolygon': 16,
    'outputmeter': 17,
    'outputlinearbargraph': 18,
    'outputarchedbargraph': 19,
    'picturegraphic': 20,
    'numbervar': 21,
    'stringvar': 22,
    'fontattributes': 23,
    'lineattributes': 24,
    'fillattributes': 25,
    'inputattributes': 26,
    'objectpointer': 27,
    'macro': 28,
    'auxiliaryfunction': 29,
    'auxiliaryinput': 30,
}

SUPPORTED_TAGS = {
    'workingset',
    'softkeymask',
    'key',
    'datamask',
    'button',
    'outputstring',
    'outputnumber',
    'fontattributes',
}

COLOURS = {
    'black': 0,
    'white': 1,
    'green': 2,
    'teal': 3,
    'maroon': 4,
    'purple': 5,
    'olive': 6,
    'silver': 7,
    'grey': 8,
    'blue': 9,
    'lime': 10,
    'cyan': 11,
    'red': 12,
    'magenta': 13,
    'yellow': 14,
    'navy': 15,
}

FONT_SIZES = {
    '6x8': 0,
    '8x8': 1,
    '8x12': 2,
    '12x16': 3,
    '16x16': 4,
    '16x24': 5,
    '24x32': 6,
    '32x32': 7,
    '32x48': 8,
    '48x64': 9,
    '64x64': 10,
    '64x96': 11,
    '96x128': 12,
    '128x128': 13,
    '128x192': 14,
}

FONT_TYPES = {
    'latin1': 0,
    'iso8859-1': 0,
    'iso8859_1': 0,
    'iso8859-15': 1,
    'iso8859_15': 1,
    'iso8859-2': 2,
    'iso8859_2': 2,
    'iso8859-4': 3,
    'iso8859_4': 3,
    'iso8859-5': 4,
    'iso8859_5': 4,
    'iso8859-7': 5,
    'iso8859_7': 5,
}

JUSTIFICATION_H = {
    'left': 0,
    'centred': 1,
    'centered': 1,
    'middle': 1,
    'right': 2,
}

JUSTIFICATION_V = {
    'top': 0,
    'middle': 1,
    'center': 1,
    'centre': 1,
    'bottom': 2,
}


class GeneratorError(RuntimeError):
    pass


def parse_id(element):
    return int(element.get('id', '0'), 10)


def parse_int(value, default=0):
    if value in (None, ''):
        return default
    return int(float(value))


def parse_float(value, default=0.0):
    if value in (None, ''):
        return default
    return float(value)


def parse_bool(value, default=False):
    if value is None:
        return default
    return str(value).strip().lower() in {'1', 'true', 'yes'}


def encode_string(value):
    return value.encode('latin-1', errors='replace')


class IOPGenerator:
    def __init__(self, xml_file):
        self.xml_file = xml_file
        self.output = bytearray()
        self.name_to_id = {}
        self.name_to_element = {}
        self.id_to_name = {}
        self.objects = []

    def load_xml(self):
        tree = ET.parse(self.xml_file)
        self.root = tree.getroot()
        if self.root.tag != 'objectpool':
            raise GeneratorError(f'Unexpected root element: {self.root.tag}')

        for elem in self.root:
            if elem.tag not in SUPPORTED_TAGS:
                raise GeneratorError(f'Unsupported object type in XML: {elem.tag}')

            name = elem.get('name')
            if not name:
                raise GeneratorError(f'Missing object name on <{elem.tag}>')

            obj_id = parse_id(elem)
            if name in self.name_to_id:
                raise GeneratorError(f'Duplicate object name: {name}')
            if obj_id in self.id_to_name:
                raise GeneratorError(f'Duplicate object ID: {obj_id}')

            self.name_to_id[name] = obj_id
            self.name_to_element[name] = elem
            self.id_to_name[obj_id] = name
            self.objects.append(elem)

        self.validate_xml_structure()

    def validate_xml_structure(self):
        working_sets = [elem for elem in self.objects if elem.tag == 'workingset']
        if len(working_sets) != 1:
            raise GeneratorError(f'Expected exactly one workingset, found {len(working_sets)}')

        for elem in self.objects:
            if elem.tag == 'workingset':
                self.require_ref(elem, elem.get('active_mask'), 'active_mask')
            elif elem.tag == 'datamask':
                self.require_ref(elem, elem.get('soft_key_mask'), 'soft_key_mask')
            elif elem.tag in {'outputstring', 'outputnumber'}:
                self.require_ref(elem, elem.get('font_attributes'), 'font_attributes')

            for child in self.get_children(elem):
                name = child.get('name')
                if not name:
                    raise GeneratorError(f'include_object without name in {elem.get("name")}')
                self.require_ref(elem, name, 'include_object')

    def require_ref(self, elem, ref_name, field_name):
        if ref_name and ref_name not in self.name_to_id:
            raise GeneratorError(
                f'Unknown reference {ref_name!r} in {field_name} for {elem.tag} {elem.get("name")!r}'
            )

    def get_children(self, element):
        return [child for child in element if child.tag == 'include_object']

    def resolve_id(self, name, allow_null=False):
        if not name:
            if allow_null:
                return 0xFFFF
            raise GeneratorError('Missing object reference')
        if name not in self.name_to_id:
            if allow_null:
                return 0xFFFF
            raise GeneratorError(f'Unknown object reference: {name}')
        return self.name_to_id[name]

    def get_child_location(self, child_name):
        referenced = self.name_to_element.get(child_name)
        if referenced is None:
            raise GeneratorError(f'Unknown child object: {child_name}')
        return parse_int(referenced.get('x'), 0), parse_int(referenced.get('y'), 0)

    def get_colour(self, value, default='black'):
        colour_name = str(value or default).strip().lower()
        if colour_name not in COLOURS:
            raise GeneratorError(f'Unknown colour: {value}')
        return COLOURS[colour_name]

    def get_font_size(self, value):
        font_size = str(value or '8x8').strip().lower()
        if font_size not in FONT_SIZES:
            raise GeneratorError(f'Unknown font size: {value}')
        return FONT_SIZES[font_size]

    def get_font_type(self, value):
        font_type = str(value or 'latin1').strip().lower()
        if font_type not in FONT_TYPES:
            raise GeneratorError(f'Unsupported font type: {value}')
        return FONT_TYPES[font_type]

    def get_font_style(self, value):
        if value in (None, '', 'normal'):
            return 0

        flags = 0
        parts = [part.strip().lower() for part in str(value).replace('|', ',').split(',') if part.strip()]
        style_bits = {
            'bold': 0,
            'crossed': 1,
            'underlined': 2,
            'italic': 3,
            'inverted': 4,
            'flashing': 5,
            'flashed': 6,
        }
        for part in parts:
            if part not in style_bits:
                raise GeneratorError(f'Unsupported font style: {value}')
            flags |= 1 << style_bits[part]
        return flags

    def get_justification(self, elem):
        horizontal = JUSTIFICATION_H.get(str(elem.get('horizontal_justification', 'left')).lower(), 0)
        vertical = JUSTIFICATION_V.get(str(elem.get('vertical_justification', 'top')).lower(), 0)
        return (vertical << 4) | horizontal

    def get_options(self, elem):
        return parse_int(elem.get('options'), 0) & 0xFF

    def get_format(self, elem):
        raw_format = str(elem.get('format', '')).strip().lower()
        if raw_format in {'', '0', '%d', '%u', '%4d', '%4u', 'fixed'}:
            return 0
        if raw_format in {'1', '%e', '%f', 'scientific', 'exponential'}:
            return 1
        return parse_int(raw_format, 0) & 0xFF

    def child_object_refs(self, elem, excluded_roles=None):
        excluded_roles = excluded_roles or set()
        refs = []
        for child in self.get_children(elem):
            if child.get('role', '') in excluded_roles:
                continue
            child_name = child.get('name', '')
            ref_id = self.resolve_id(child_name)
            x_pos, y_pos = self.get_child_location(child_name)
            refs.append((ref_id, x_pos, y_pos))
        return refs

    def write_uint8(self, value):
        self.output += struct.pack('<B', int(value) & 0xFF)

    def write_int16(self, value):
        self.output += struct.pack('<h', int(value))

    def write_uint16(self, value):
        self.output += struct.pack('<H', int(value) & 0xFFFF)

    def write_uint32(self, value):
        self.output += struct.pack('<I', int(value) & 0xFFFFFFFF)

    def encode_workingset(self, elem):
        children = self.child_object_refs(elem, excluded_roles={'active_mask'})
        self.write_uint16(parse_id(elem))
        self.write_uint8(OBJECT_TYPES['workingset'])
        self.write_uint8(self.get_colour(elem.get('background_colour', 'black')))
        self.write_uint8(1 if parse_bool(elem.get('selectable'), True) else 0)
        self.write_uint16(self.resolve_id(elem.get('active_mask')))
        self.write_uint8(len(children))
        self.write_uint8(0)
        self.write_uint8(0)
        for ref_id, x_pos, y_pos in children:
            self.write_uint16(ref_id)
            self.write_int16(x_pos)
            self.write_int16(y_pos)

    def encode_datamask(self, elem):
        children = self.child_object_refs(elem, excluded_roles={'soft_key_mask'})
        self.write_uint16(parse_id(elem))
        self.write_uint8(OBJECT_TYPES['datamask'])
        self.write_uint8(self.get_colour(elem.get('background_colour', 'black')))
        self.write_uint16(self.resolve_id(elem.get('soft_key_mask')))
        self.write_uint8(len(children))
        self.write_uint8(0)
        for ref_id, x_pos, y_pos in children:
            self.write_uint16(ref_id)
            self.write_int16(x_pos)
            self.write_int16(y_pos)

    def encode_softkeymask(self, elem):
        self.write_uint16(parse_id(elem))
        self.write_uint8(OBJECT_TYPES['softkeymask'])
        self.write_uint8(self.get_colour(elem.get('background_colour', 'black')))
        children = [self.resolve_id(child.get('name')) for child in self.get_children(elem)]
        self.write_uint8(len(children))
        self.write_uint8(0)
        for ref_id in children:
            self.write_uint16(ref_id)

    def encode_key(self, elem):
        children = self.child_object_refs(elem)
        self.write_uint16(parse_id(elem))
        self.write_uint8(OBJECT_TYPES['key'])
        self.write_uint8(self.get_colour(elem.get('background_colour', 'black')))
        self.write_uint8(parse_int(elem.get('key_code'), 0))
        self.write_uint8(len(children))
        self.write_uint8(0)
        for ref_id, x_pos, y_pos in children:
            self.write_uint16(ref_id)
            self.write_int16(x_pos)
            self.write_int16(y_pos)

    def encode_button(self, elem):
        children = self.child_object_refs(elem)
        self.write_uint16(parse_id(elem))
        self.write_uint8(OBJECT_TYPES['button'])
        self.write_uint16(parse_int(elem.get('width'), 0))
        self.write_uint16(parse_int(elem.get('height'), 0))
        self.write_uint8(self.get_colour(elem.get('background_colour', 'black')))
        self.write_uint8(self.get_colour(elem.get('border_colour', 'white')))
        self.write_uint8(parse_int(elem.get('key_code'), 0))
        self.write_uint8(self.get_options(elem))
        self.write_uint8(len(children))
        self.write_uint8(0)
        for ref_id, x_pos, y_pos in children:
            self.write_uint16(ref_id)
            self.write_int16(x_pos)
            self.write_int16(y_pos)

    def encode_outputstring(self, elem):
        value = encode_string(elem.get('value', ''))
        self.write_uint16(parse_id(elem))
        self.write_uint8(OBJECT_TYPES['outputstring'])
        self.write_uint16(parse_int(elem.get('width'), 0))
        self.write_uint16(parse_int(elem.get('height'), 0))
        self.write_uint8(self.get_colour(elem.get('background_colour', 'black')))
        self.write_uint16(self.resolve_id(elem.get('font_attributes')))
        self.write_uint8(self.get_options(elem))
        self.write_uint16(0xFFFF)
        self.write_uint8(self.get_justification(elem))
        self.write_uint16(len(value))
        self.output += value
        self.write_uint8(0)

    def encode_outputnumber(self, elem):
        self.write_uint16(parse_id(elem))
        self.write_uint8(OBJECT_TYPES['outputnumber'])
        self.write_uint16(parse_int(elem.get('width'), 0))
        self.write_uint16(parse_int(elem.get('height'), 0))
        self.write_uint8(self.get_colour(elem.get('background_colour', 'black')))
        self.write_uint16(self.resolve_id(elem.get('font_attributes')))
        self.write_uint8(self.get_options(elem))
        self.write_uint16(0xFFFF)
        self.write_uint32(parse_int(elem.get('value'), 0))
        self.write_uint32(parse_int(elem.get('offset'), 0))
        self.output += struct.pack('<f', parse_float(elem.get('scale'), 1.0))
        self.write_uint8(parse_int(elem.get('number_of_decimals'), 0))
        self.write_uint8(self.get_format(elem))
        self.write_uint8(self.get_justification(elem))
        self.write_uint8(0)

    def encode_fontattributes(self, elem):
        self.write_uint16(parse_id(elem))
        self.write_uint8(OBJECT_TYPES['fontattributes'])
        self.write_uint8(self.get_colour(elem.get('font_colour', 'white'), 'white'))
        self.write_uint8(self.get_font_size(elem.get('font_size', '8x8')))
        self.write_uint8(self.get_font_type(elem.get('font_type', 'latin1')))
        self.write_uint8(self.get_font_style(elem.get('font_style', 'normal')))
        self.write_uint8(0)

    def validate_serialized_pool(self):
        data = self.output
        index = 0
        object_count = 0
        while index < len(data):
            if len(data) - index < 3:
                raise GeneratorError('Serialized pool ended mid-object header')

            obj_type = data[index + 2]
            object_count += 1

            if obj_type == OBJECT_TYPES['workingset']:
                if len(data) - index < 10:
                    raise GeneratorError('Working set truncated')
                children = data[index + 7]
                index += 10 + (children * 6)
            elif obj_type == OBJECT_TYPES['datamask']:
                if len(data) - index < 8:
                    raise GeneratorError('Data mask truncated')
                children = data[index + 6]
                index += 8 + (children * 6)
            elif obj_type == OBJECT_TYPES['softkeymask']:
                if len(data) - index < 6:
                    raise GeneratorError('Soft key mask truncated')
                children = data[index + 4]
                index += 6 + (children * 2)
            elif obj_type == OBJECT_TYPES['key']:
                if len(data) - index < 7:
                    raise GeneratorError('Key truncated')
                children = data[index + 5]
                index += 7 + (children * 6)
            elif obj_type == OBJECT_TYPES['button']:
                if len(data) - index < 13:
                    raise GeneratorError('Button truncated')
                children = data[index + 11]
                index += 13 + (children * 6)
            elif obj_type == OBJECT_TYPES['outputstring']:
                if len(data) - index < 16:
                    raise GeneratorError('Output string truncated')
                string_length = struct.unpack_from('<H', data, index + 14)[0]
                index += 17 + string_length
            elif obj_type == OBJECT_TYPES['outputnumber']:
                if len(data) - index < 29:
                    raise GeneratorError('Output number truncated')
                index += 29
            elif obj_type == OBJECT_TYPES['fontattributes']:
                if len(data) - index < 8:
                    raise GeneratorError('Font attributes truncated')
                index += 8
            else:
                raise GeneratorError(f'Unsupported serialized object type {obj_type} at byte {index}')

            if index > len(data):
                raise GeneratorError('Serialized pool length overflowed object boundary')

        if object_count != len(self.objects):
            raise GeneratorError(
                f'Serialized object count mismatch: encoded {object_count}, expected {len(self.objects)}'
            )

    def generate(self, output_file):
        print(f'Loading XML: {self.xml_file}')
        self.load_xml()
        print(f'Found {len(self.objects)} objects')
        print(f'Generating binary .iop: {output_file}')

        encoders = {
            'workingset': self.encode_workingset,
            'softkeymask': self.encode_softkeymask,
            'key': self.encode_key,
            'datamask': self.encode_datamask,
            'button': self.encode_button,
            'outputstring': self.encode_outputstring,
            'outputnumber': self.encode_outputnumber,
            'fontattributes': self.encode_fontattributes,
        }

        for elem in self.objects:
            encoders[elem.tag](elem)

        self.validate_serialized_pool()

        with open(output_file, 'wb') as output_handle:
            output_handle.write(self.output)

        print(f'Success! Generated {len(self.output)} bytes')
        print(f'Output: {output_file}')


if __name__ == '__main__':
    script_dir = os.path.dirname(os.path.abspath(__file__))
    xml_input = os.path.join(script_dir, 'object_pool')
    iop_output = os.path.join(script_dir, 'examples', '1200PT', 'src', 'object_pool', 'object_pool.iop')

    if not os.path.exists(xml_input):
        print(f'ERROR: XML file not found: {xml_input}')
        sys.exit(1)

    os.makedirs(os.path.dirname(iop_output), exist_ok=True)

    try:
        generator = IOPGenerator(xml_input)
        generator.generate(iop_output)
    except GeneratorError as error:
        print(f'ERROR: {error}')
        sys.exit(1)
