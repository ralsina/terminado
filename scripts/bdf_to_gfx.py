#!/usr/bin/env python3
"""
Convert BDF font files to Adafruit GFX C header format.

This script parses BDF (Glyph Bitmap Distribution Format) font files
and generates C header files compatible with the Adafruit GFX library.
"""

import argparse
import re
import sys
from pathlib import Path
from typing import List, Dict, Tuple, Optional


class BDFGlyph:
    """Represents a single glyph from a BDF font."""

    def __init__(self):
        self.encoding = 0
        self.width = 0
        self.height = 0
        self.x_offset = 0
        self.y_offset = 0
        self.dwidth = 0
        self.bitmap: List[int] = []
        self.name = ""


class BDFParser:
    """Parser for BDF font files."""

    def __init__(self, filepath: str):
        self.filepath = filepath
        self.glyphs: Dict[int, BDFGlyph] = {}
        self.font_name = ""
        self.font_bounding_box: Tuple[int, int, int, int] = (0, 0, 0, 0)
        self._parse()

    def _parse(self):
        """Parse the BDF file."""
        current_glyph = None
        in_bitmap = False
        bitmap_rows = []

        with open(self.filepath, 'r') as f:
            for line in f:
                line = line.strip()

                # Parse font metadata
                if line.startswith('FONT '):
                    parts = line.split()
                    if len(parts) > 1:
                        # Extract font name, clean up the BDF font name
                        font_full_name = parts[1]
                        # Take the first part or use a cleaned version
                        self.font_name = font_full_name.split('-')[0].replace('misc', 'spleen')
                elif line.startswith('FONTBOUNDINGBOX'):
                    parts = line.split()
                    self.font_bounding_box = tuple(map(int, parts[1:5]))

                # Start of a character
                elif line.startswith('STARTCHAR'):
                    current_glyph = BDFGlyph()
                    current_glyph.name = line.split()[1]
                    in_bitmap = False
                    bitmap_rows = []

                # Character encoding (ASCII/Unicode value)
                elif line.startswith('ENCODING') and current_glyph:
                    current_glyph.encoding = int(line.split()[1])

                # Device width (advance width)
                elif line.startswith('DWIDTH') and current_glyph:
                    current_glyph.dwidth = int(line.split()[1])

                # Bounding box for this character
                elif line.startswith('BBX') and current_glyph:
                    parts = line.split()
                    current_glyph.width = int(parts[1])
                    current_glyph.height = int(parts[2])
                    current_glyph.x_offset = int(parts[3])
                    current_glyph.y_offset = int(parts[4])

                # Start of bitmap data
                elif line == 'BITMAP' and current_glyph:
                    in_bitmap = True
                    bitmap_rows = []

                # End of character
                elif line == 'ENDCHAR' and current_glyph:
                    # Convert hex bitmap rows to integer list
                    current_glyph.bitmap = self._parse_bitmap(bitmap_rows, current_glyph.width)
                    self.glyphs[current_glyph.encoding] = current_glyph
                    current_glyph = None
                    in_bitmap = False

                # Bitmap data rows
                elif in_bitmap and current_glyph and re.match(r'^[0-9A-Fa-f]+$', line):
                    bitmap_rows.append(line)

    def _parse_bitmap(self, hex_rows: List[str], width: int) -> List[int]:
        """Convert hex bitmap rows to Adafruit GFX bitmap format (packed bits, row-major)."""
        if not hex_rows:
            return []

        # BDF pads each row to an 8-bit boundary; total bits in each row:
        bytes_per_bdf_row = (width + 7) // 8
        total_bits_in_bdf_row = bytes_per_bdf_row * 8

        # Pack pixels continuously: row by row, left to right, MSB first.
        # GFX drawChar reads bits this way with NO padding between rows.
        result = []
        current_byte = 0
        bit_count = 0

        for row_hex in hex_rows:
            row_value = int(row_hex, 16)
            for col in range(width):
                # BDF stores leftmost pixel at the highest bit of the padded row
                pixel = (row_value >> (total_bits_in_bdf_row - 1 - col)) & 1
                current_byte = (current_byte << 1) | pixel
                bit_count += 1
                if bit_count == 8:
                    result.append(current_byte)
                    current_byte = 0
                    bit_count = 0

        # Flush any remaining bits (left-align in the last byte)
        if bit_count > 0:
            result.append(current_byte << (8 - bit_count))

        return result


class GFXGenerator:
    """Generate Adafruit GFX C header files from BDF data."""

    def __init__(self, parser: BDFParser, first_glyph=None, last_glyph=None):
        self.parser = parser
        # Filter glyphs by range if specified
        all_glyphs = sorted(parser.glyphs.values(), key=lambda g: g.encoding)
        if first_glyph is not None or last_glyph is not None:
            start = first_glyph if first_glyph is not None else all_glyphs[0].encoding if all_glyphs else 32
            end = last_glyph if last_glyph is not None else all_glyphs[-1].encoding if all_glyphs else 126
            self.glyph_list = [g for g in all_glyphs if start <= g.encoding <= end]
        else:
            self.glyph_list = all_glyphs

    def generate(self, output_path: str, input_path):
        """Generate the C header file."""
        # Create contiguous bitmap array
        bitmap_data = []
        glyph_offsets = {}

        for glyph in self.glyph_list:
            glyph_offsets[glyph.encoding] = len(bitmap_data)
            bitmap_data.extend(glyph.bitmap)

        # Generate the header file content
        content = self._generate_header(bitmap_data, glyph_offsets, input_path)

        # Write to file
        with open(output_path, 'w') as f:
            f.write(content)

        # Print range info
        range_info = ""
        if len(self.glyph_list) > 0:
            first = self.glyph_list[0].encoding
            last = self.glyph_list[-1].encoding
            range_info = f" (range: 0x{first:02X}-0x{last:02X})"

        print(f"Generated: {output_path}")
        print(f"  Font: {self.parser.font_name}")
        print(f"  Characters: {len(self.glyph_list)}{range_info}")
        print(f"  Size: {self.parser.font_bounding_box[0]}x{self.parser.font_bounding_box[1]}")

    def _generate_header(self, bitmap_data: List[int], glyph_offsets: Dict[int, int], input_path) -> str:
        """Generate the C header file content."""

        # Generate a proper font name from the input filename
        base_name = input_path.stem if hasattr(input_path, 'stem') else self.parser.font_name
        if not base_name:
            base_name = "font"

        font_name_c = base_name.replace('-', '_')

        lines = [
            f"/**",
            f"** {base_name} font for Adafruit GFX",
            f"**",
            f"** Converted from {self.parser.filepath.name}",
            f"** Font size: {self.parser.font_bounding_box[0]}x{self.parser.font_bounding_box[1]}",
            f"** Characters: {len(self.glyph_list)}",
            f"**/",
            "",
            f"const uint8_t {font_name_c}Bitmaps[] PROGMEM = {{",
        ]

        # Format bitmap data as hex (12 bytes per line)
        for i, byte in enumerate(bitmap_data):
            if i % 12 == 0:
                lines.append(f"  0x{byte:02X}")
            else:
                lines[-1] += f", 0x{byte:02X}"

            # Add comma at end of each line (except the last one)
            if (i + 1) % 12 == 0 and i + 1 < len(bitmap_data):
                lines[-1] += ","

        lines.append("};")
        lines.append("")
        lines.append(f"const GFXglyph {font_name_c}Glyphs[] PROGMEM = {{")

        # Generate glyph descriptors
        for i, glyph in enumerate(self.glyph_list):
            bitmap_offset = glyph_offsets[glyph.encoding]
            # GFXglyph format: {bitmapOffset, width, height, xAdvance, xOffset, yOffset}
            # GFX yOffset = distance (downward) from cursor to top of glyph.
            # BDF top row is at (y_offset + height - 1) pixels ABOVE the baseline,
            # so GFX yOffset = -(y_offset + height - 1).
            y_offset = -(glyph.y_offset + glyph.height - 1)

            char_repr = chr(glyph.encoding) if 32 <= glyph.encoding < 127 else '?'

            if i == 0:
                # First entry - no comma before it
                lines.append(f"  {{ {bitmap_offset:5d},   {glyph.width:2d},   {glyph.height:2d},   {glyph.dwidth:2d},    {glyph.x_offset:2d}, {y_offset:4d} }}   // '{char_repr}'")
            else:
                # Subsequent entries - comma before them
                lines.append(f" ,{{ {bitmap_offset:5d},   {glyph.width:2d},   {glyph.height:2d},   {glyph.dwidth:2d},    {glyph.x_offset:2d}, {y_offset:4d} }}   // '{char_repr}'")

        lines.append("};")
        lines.append("")
        lines.append(f"const GFXfont {font_name_c} PROGMEM = {{")
        lines.append(f"  (uint8_t  *){font_name_c}Bitmaps,")
        lines.append(f"  (GFXglyph *){font_name_c}Glyphs,")

        if self.glyph_list:
            first_glyph = self.glyph_list[0].encoding
            last_glyph = self.glyph_list[-1].encoding
            y_advance = self.parser.font_bounding_box[1]
        else:
            first_glyph = 32
            last_glyph = 126
            y_advance = 8

        lines.append(f"  0x{first_glyph:02X}, 0x{last_glyph:02X}, {y_advance}}};")

        return "\n".join(lines) + "\n"


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Convert BDF font files to Adafruit GFX C header format"
    )
    parser.add_argument(
        "input",
        help="Input BDF font file"
    )
    parser.add_argument(
        "-o", "--output",
        help="Output C header file (default: input_filename.h)",
        default=None
    )
    parser.add_argument(
        "-f", "--first",
        type=int,
        help="First glyph encoding to include (default: first available glyph)",
        default=None
    )
    parser.add_argument(
        "-l", "--last",
        type=int,
        help="Last glyph encoding to include (default: last available glyph)",
        default=None
    )

    args = parser.parse_args()

    # Validate glyph range
    if args.first is not None and args.last is not None:
        if args.first > args.last:
            print(f"Error: First glyph ({args.first}) cannot be greater than last glyph ({args.last})", file=sys.stderr)
            sys.exit(1)

    # Validate input file
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"Error: Input file '{args.input}' not found", file=sys.stderr)
        sys.exit(1)

    # Determine output path
    if args.output:
        output_path = Path(args.output)
    else:
        output_path = input_path.with_suffix('.h')

    # Parse BDF file
    print(f"Parsing BDF file: {input_path}")
    try:
        bdf_parser = BDFParser(input_path)
    except Exception as e:
        print(f"Error parsing BDF file: {e}", file=sys.stderr)
        sys.exit(1)

    # Generate GFX header
    try:
        generator = GFXGenerator(bdf_parser, first_glyph=args.first, last_glyph=args.last)
        generator.generate(output_path, input_path)
    except Exception as e:
        print(f"Error generating GFX file: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()