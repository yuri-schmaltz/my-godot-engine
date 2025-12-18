#!/usr/bin/env python3
"""
Color Contrast Auditor for Godot Engine
Validates WCAG 2.1 AA compliance (4.5:1 for text, 3:1 for UI components)

Usage:
    python check_color_contrast.py [--fix] [--theme-file path/to/theme.cpp]
    
Examples:
    # Audit default theme
    python check_color_contrast.py
    
    # Audit specific theme file
    python check_color_contrast.py --theme-file editor/themes/theme_classic.cpp
    
    # Auto-fix contrast issues (generates suggestions)
    python check_color_contrast.py --fix
"""

import argparse
import math
import re
import sys
from pathlib import Path
from typing import Dict, List, Tuple


class Color:
    """Represents an RGBA color."""

    def __init__(self, r: float, g: float, b: float, a: float = 1.0):
        self.r = max(0.0, min(1.0, r))
        self.g = max(0.0, min(1.0, g))
        self.b = max(0.0, min(1.0, b))
        self.a = max(0.0, min(1.0, a))

    @classmethod
    def from_hex(cls, hex_str: str) -> "Color":
        """Create Color from hex string (#RRGGBB or #RRGGBBAA)."""
        hex_str = hex_str.lstrip("#")
        if len(hex_str) == 6:
            r, g, b = [int(hex_str[i : i + 2], 16) / 255.0 for i in (0, 2, 4)]
            return cls(r, g, b, 1.0)
        elif len(hex_str) == 8:
            r, g, b, a = [int(hex_str[i : i + 2], 16) / 255.0 for i in (0, 2, 4, 6)]
            return cls(r, g, b, a)
        else:
            raise ValueError(f"Invalid hex color: {hex_str}")

    @classmethod
    def from_cpp_constructor(cls, cpp_str: str) -> "Color":
        """Parse Color from C++ constructor like 'Color(0.5, 0.3, 0.1)'."""
        match = re.search(r"Color\s*\(\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)(?:\s*,\s*([\d.]+))?\s*\)", cpp_str)
        if match:
            r, g, b = float(match.group(1)), float(match.group(2)), float(match.group(3))
            a = float(match.group(4)) if match.group(4) else 1.0
            return cls(r, g, b, a)
        raise ValueError(f"Invalid C++ Color constructor: {cpp_str}")

    def relative_luminance(self) -> float:
        """Calculate relative luminance per WCAG 2.1."""

        def adjust(c: float) -> float:
            return c / 12.92 if c <= 0.03928 else math.pow((c + 0.055) / 1.055, 2.4)

        return 0.2126 * adjust(self.r) + 0.7152 * adjust(self.g) + 0.0722 * adjust(self.b)

    def to_hex(self) -> str:
        """Convert to hex string."""
        r, g, b = int(self.r * 255), int(self.g * 255), int(self.b * 255)
        return f"#{r:02X}{g:02X}{b:02X}"

    def __repr__(self) -> str:
        return f"Color({self.r:.3f}, {self.g:.3f}, {self.b:.3f}, {self.a:.3f})"


def calculate_contrast_ratio(color1: Color, color2: Color) -> float:
    """Calculate WCAG 2.1 contrast ratio between two colors."""
    l1 = color1.relative_luminance()
    l2 = color2.relative_luminance()
    lighter = max(l1, l2)
    darker = min(l1, l2)
    return (lighter + 0.05) / (darker + 0.05)


def meets_wcag_aa_text(ratio: float) -> bool:
    """Check if ratio meets WCAG AA for normal text (4.5:1)."""
    return ratio >= 4.5


def meets_wcag_aa_large_text(ratio: float) -> bool:
    """Check if ratio meets WCAG AA for large text (3:1)."""
    return ratio >= 3.0


def meets_wcag_aa_ui(ratio: float) -> bool:
    """Check if ratio meets WCAG AA for UI components (3:1)."""
    return ratio >= 3.0


def adjust_color_for_contrast(fg: Color, bg: Color, target_ratio: float = 4.5) -> Color:
    """Adjust foreground color to meet target contrast ratio."""
    fg_lum = fg.relative_luminance()
    bg_lum = bg.relative_luminance()

    # Determine if we need lighter or darker foreground
    if fg_lum > bg_lum:
        # Foreground is lighter - make it lighter
        target_lum = (target_ratio * (bg_lum + 0.05)) - 0.05
    else:
        # Foreground is darker - make it darker
        target_lum = ((bg_lum + 0.05) / target_ratio) - 0.05

    # Clamp target luminance
    target_lum = max(0.0, min(1.0, target_lum))

    # Simple adjustment: scale RGB proportionally
    # This is a heuristic and may not always produce perfect results
    current_lum = fg_lum
    if current_lum == 0:
        # Pure black, adjust to target
        scale = target_lum
    else:
        scale = target_lum / current_lum

    adjusted = Color(
        min(1.0, fg.r * scale),
        min(1.0, fg.g * scale),
        min(1.0, fg.b * scale),
        fg.a,
    )

    return adjusted


def parse_theme_file(file_path: Path) -> List[Dict]:
    """Parse a Godot theme .cpp file for color definitions."""
    if not file_path.exists():
        print(f"Error: Theme file not found: {file_path}")
        return []

    content = file_path.read_text(encoding="utf-8")
    color_pairs = []

    # Pattern to find color definitions
    # Example: p_config->set_color("font_color", "Button", Color(0.875, 0.875, 0.875))
    pattern = r'set_color\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*(Color\([^)]+\))'

    for match in re.finditer(pattern, content):
        color_name = match.group(1)
        widget_name = match.group(2)
        color_str = match.group(3)

        try:
            color = Color.from_cpp_constructor(color_str)
            color_pairs.append(
                {
                    "widget": widget_name,
                    "property": color_name,
                    "color": color,
                    "line": content[: match.start()].count("\n") + 1,
                    "original": match.group(0),
                }
            )
        except ValueError as e:
            print(f"Warning: Could not parse color at line {content[:match.start()].count('\n') + 1}: {e}")

    return color_pairs


def audit_contrast(theme_colors: List[Dict], background_color: Color) -> List[Dict]:
    """Audit color contrast and return violations."""
    violations = []

    for entry in theme_colors:
        color = entry["color"]
        property_name = entry["property"]

        # Determine if this is text or UI component
        is_text = "font" in property_name.lower() or "text" in property_name.lower()
        is_ui = "border" in property_name.lower() or "outline" in property_name.lower()

        ratio = calculate_contrast_ratio(color, background_color)

        passed = False
        requirement = ""

        if is_text:
            # Check for normal text (4.5:1)
            if meets_wcag_aa_text(ratio):
                passed = True
            else:
                requirement = "WCAG AA Text (4.5:1)"
        elif is_ui:
            # Check for UI components (3:1)
            if meets_wcag_aa_ui(ratio):
                passed = True
            else:
                requirement = "WCAG AA UI (3:1)"
        else:
            # Default to UI requirement
            if meets_wcag_aa_ui(ratio):
                passed = True
            else:
                requirement = "WCAG AA (3:1)"

        if not passed:
            target_ratio = 4.5 if is_text else 3.0
            suggested_color = adjust_color_for_contrast(color, background_color, target_ratio)

            violations.append(
                {
                    "widget": entry["widget"],
                    "property": property_name,
                    "line": entry["line"],
                    "current_color": color,
                    "current_ratio": ratio,
                    "requirement": requirement,
                    "suggested_color": suggested_color,
                    "original": entry["original"],
                }
            )

    return violations


def print_report(violations: List[Dict], show_fixes: bool = False):
    """Print contrast audit report."""
    if not violations:
        print("\n✅ All colors pass WCAG 2.1 AA contrast requirements!")
        return

    print(f"\n❌ Found {len(violations)} contrast violations:\n")
    print("=" * 100)

    for i, v in enumerate(violations, 1):
        print(f"\n{i}. {v['widget']}.{v['property']} (Line {v['line']})")
        print(f"   Current:  {v['current_color'].to_hex()} (ratio: {v['current_ratio']:.2f}:1)")
        print(f"   Required: {v['requirement']}")

        if show_fixes:
            print(f"   Suggested: {v['suggested_color'].to_hex()} " f"(ratio: {calculate_contrast_ratio(v['suggested_color'], Color(0.2, 0.2, 0.2)):.2f}:1)")
            print(f"   Original:  {v['original']}")
            print(
                f"   Fixed:     set_color(\"{v['property']}\", \"{v['widget']}\", "
                f"Color({v['suggested_color'].r:.3f}, {v['suggested_color'].g:.3f}, "
                f"{v['suggested_color'].b:.3f}))"
            )

        print("-" * 100)

    print(f"\nTotal violations: {len(violations)}")
    print("Run with --fix to see suggested corrections.")


def main():
    parser = argparse.ArgumentParser(description="Audit color contrast in Godot theme files")
    parser.add_argument("--theme-file", type=str, help="Path to theme .cpp file to audit")
    parser.add_argument("--fix", action="store_true", help="Show suggested fixes for violations")
    parser.add_argument("--background", type=str, default="#333333", help="Background color as hex (default: #333333)")

    args = parser.parse_args()

    # Determine theme file to audit
    if args.theme_file:
        theme_path = Path(args.theme_file)
    else:
        # Default to editor classic theme
        theme_path = Path(__file__).parent.parent.parent / "editor" / "themes" / "theme_classic.cpp"

    print(f"Auditing theme file: {theme_path}")
    print(f"Background color: {args.background}")

    # Parse theme colors
    theme_colors = parse_theme_file(theme_path)
    if not theme_colors:
        print("No colors found in theme file.")
        return 1

    print(f"Found {len(theme_colors)} color definitions.")

    # Parse background color
    try:
        background = Color.from_hex(args.background)
    except ValueError:
        print(f"Invalid background color: {args.background}")
        return 1

    # Audit contrast
    violations = audit_contrast(theme_colors, background)

    # Print report
    print_report(violations, show_fixes=args.fix)

    # Return exit code
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())
