#!/usr/bin/env python3
"""
generate_aidl_headers.py

Converts Android AIDL enum and parcelable definitions from aidl_property/
into C++ headers consumable on Linux/QNX without the Android AIDL compiler.

Supported AIDL constructs:
  - @Backing(type="int"|"long"|"byte") enum Foo { ... }
  - parcelable Foo { field declarations }
  - import statements (converted to #include)
  - Inline /** */ and // comments

Usage:
  python3 generate_aidl_headers.py \
      --input  <aidl_property_dir> \
      --output <output_header_dir>

Output structure mirrors the input:
  input:  aidl_property/android/hardware/automotive/vehicle/VehicleGear.aidl
  output: <out>/aidl/android/hardware/automotive/vehicle/VehicleGear.h
"""

import argparse
import os
import re
import sys


# ──────────────────────────────────────────────────────────────────────────────
# Backing type map: AIDL type → C++ stdint type
# ──────────────────────────────────────────────────────────────────────────────
BACKING_TYPE_MAP = {
    "byte":  "int8_t",
    "int":   "int32_t",
    "long":  "int64_t",
}

# ──────────────────────────────────────────────────────────────────────────────
# AIDL field type → C++ type
# ──────────────────────────────────────────────────────────────────────────────
FIELD_TYPE_MAP = {
    "boolean":  "bool",
    "byte":     "uint8_t",  # byte arrays are unsigned in practice
    "short":    "int16_t",
    "int":      "int32_t",
    "long":     "int64_t",
    "float":    "float",
    "double":   "double",
    "String":   "std::string",
    "char":     "char16_t",
}


def aidl_type_to_cpp(aidl_type: str, package_prefix: str = "aidl") -> str:
    """Convert an AIDL type reference to its C++ equivalent."""
    # Strip annotations like @utf8InCpp
    aidl_type = re.sub(r"@\w+\s*", "", aidl_type).strip()

    # Handle AIDL array syntax: int[] → std::vector<int32_t>
    # Must check before scalar lookup so "int[]" doesn't fall through
    if aidl_type.endswith("[]"):
        base = aidl_type[:-2].strip()
        cpp_base = aidl_type_to_cpp(base, package_prefix)
        return f"std::vector<{cpp_base}>"

    # Handle generic List<T> → std::vector<T>
    list_m = re.match(r"List<(.+)>$", aidl_type)
    if list_m:
        cpp_inner = aidl_type_to_cpp(list_m.group(1).strip(), package_prefix)
        return f"std::vector<{cpp_inner}>"

    if aidl_type in FIELD_TYPE_MAP:
        return FIELD_TYPE_MAP[aidl_type]

    # Fully qualified: android.hardware.automotive.vehicle.Foo
    if "." in aidl_type:
        return "::" + package_prefix + "::" + aidl_type.replace(".", "::")

    # Unqualified — assume same package
    return aidl_type


def package_to_namespace(package: str) -> list:
    """Split 'android.hardware.automotive.vehicle' into namespace parts."""
    return package.strip().split(".")


def import_to_include(import_stmt: str) -> str:
    """
    Convert 'android.hardware.automotive.vehicle.Foo'
    to     'aidl/android/hardware/automotive/vehicle/Foo.h'
    """
    path = import_stmt.strip().replace(".", "/")
    return f"aidl/{path}.h"


def make_include_guard(rel_path: str) -> str:
    """Turn a relative path like aidl/android/.../VehicleGear.h into a guard macro."""
    guard = rel_path.upper().replace("/", "_").replace(".", "_").replace("-", "_")
    return f"VHAL_AIDL_{guard}_"


def extract_block(text: str, start: int) -> tuple:
    """
    Given text and position of an opening '{', extract the content
    up to and including the matching '}'.
    Returns (content_inside_braces, position_after_closing_brace).
    """
    depth = 0
    i = start
    while i < len(text):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start + 1:i], i + 1
        i += 1
    return "", len(text)


def strip_comments_for_parsing(text: str) -> str:
    """
    Remove block comments and line comments for structural parsing,
    but preserve line count by replacing with whitespace.
    """
    # Replace block comments with spaces (preserve newlines)
    def replace_block(m):
        return re.sub(r"[^\n]", " ", m.group(0))

    text = re.sub(r"/\*.*?\*/", replace_block, text, flags=re.DOTALL)
    # Replace line comments
    text = re.sub(r"//[^\n]*", "", text)
    return text


def parse_enum_values(block: str) -> list:
    """
    Parse enum body into list of (name, value_str, doc_comment) tuples.
    Handles hex, decimal, expressions, and /** */ doc comments.
    """
    results = []
    # Split on commas, but be careful of comments
    # First collect doc comments and value lines together
    lines = block.split("\n")
    pending_comment = []
    pending_value = []

    for line in lines:
        stripped = line.strip()
        if not stripped:
            continue

        # Accumulate doc comment lines
        if stripped.startswith("/**") or stripped.startswith("/*"):
            pending_comment = [stripped]
            continue
        if stripped.startswith("*") and pending_comment:
            pending_comment.append(stripped)
            continue
        if stripped == "*/" or stripped.endswith("*/"):
            if pending_comment:
                pending_comment.append(stripped)
            continue

        # Line comment
        if stripped.startswith("//"):
            pending_comment.append(stripped)
            continue

        # Value line — may end with comma
        stripped = stripped.rstrip(",").strip()
        if not stripped:
            continue

        # Match: NAME = VALUE or NAME = VALUE, // comment
        m = re.match(r"^(\w+)\s*=\s*([^,/\n]+?)(?:\s*//.*)?$", stripped)
        if m:
            name = m.group(1).strip()
            value = m.group(2).strip()
            # Convert AIDL dot enum refs in value expressions: Foo.BAR -> (int32_t)Foo::BAR
            # e.g. VehicleArea.GLOBAL | VehiclePropertyType.INT32
            # Cast to int32_t to allow mixing different enum class types in bitwise OR
            value = re.sub(
                r"([A-Z]\w+)\.([A-Z_][A-Z0-9_]*)",
                r"static_cast<int32_t>(\1::\2)",
                value
            )
            doc = " ".join(pending_comment) if pending_comment else ""
            results.append((name, value, doc))
            pending_comment = []
        elif re.match(r"^\w+$", stripped):
            # Name only (auto-increment) — rare in AIDL but handle it
            doc = " ".join(pending_comment) if pending_comment else ""
            results.append((stripped, None, doc))
            pending_comment = []

    return results


def parse_parcelable_fields(block: str, import_map: dict = None) -> list:
    """
    Parse parcelable body into list of (cpp_type, name, default_value, doc) tuples.
    import_map: simple_name -> fully_qualified (e.g. ParcelFileDescriptor -> android.os.ParcelFileDescriptor)
    """
    if import_map is None:
        import_map = {}
    results = []
    # Remove annotations on field lines like @utf8InCpp
    clean = re.sub(r"@\w+(?:\([^)]*\))?\s*", "", block)
    lines = clean.split("\n")
    pending_comment = []

    for line in lines:
        stripped = line.strip()
        if not stripped:
            continue

        if stripped.startswith("/**") or stripped.startswith("/*") or \
           stripped.startswith("*") or stripped.startswith("//"):
            pending_comment.append(stripped)
            continue

        # Const field: const int FOO = value;  → static constexpr int32_t FOO = value;
        const_m = re.match(r"^const\s+([\w]+)\s+(\w+)\s*=\s*([^;]+?)\s*;", stripped)
        if const_m:
            raw_type = const_m.group(1).strip()
            name = const_m.group(2).strip()
            value = const_m.group(3).strip()
            cpp_type = aidl_type_to_cpp(raw_type)
            doc = " ".join(pending_comment) if pending_comment else ""
            # Mark as const with sentinel default so emitter treats it as static constexpr
            results.append((f"static constexpr {cpp_type}", name, value, doc))
            pending_comment = []
            continue

        # Field declaration: type name; or type name = default;
        # Handles: int foo; float[] bar = {}; String baz = "x";
        m = re.match(r"^([\w.<>]+(?:\[\])?)\s+(\w+)\s*(?:=\s*([^;]+?))?\s*;", stripped)
        if m:
            raw_type = m.group(1).strip()
            name = m.group(2).strip()
            default = m.group(3).strip() if m.group(3) else None

            # Convert AIDL enum default value syntax: Foo.BAR → Foo::BAR
            # e.g. VehiclePropertyStatus.AVAILABLE → VehiclePropertyStatus::AVAILABLE
            if default and re.match(r"^[A-Z]\w+\.[A-Z_][A-Z0-9_]*$", default):
                default = default.replace(".", "::")

            # Resolve unqualified imported types e.g. ParcelFileDescriptor
            resolved_type = raw_type
            if raw_type in import_map:
                resolved_type = import_map[raw_type]
            cpp_type = aidl_type_to_cpp(resolved_type)
            doc = " ".join(pending_comment) if pending_comment else ""
            results.append((cpp_type, name, default, doc))
            pending_comment = []

    return results


def generate_header(aidl_path: str, input_root: str, output_root: str) -> bool:
    """
    Parse one .aidl file and write the corresponding .h file.
    Returns True on success.
    """
    with open(aidl_path, "r", encoding="utf-8") as f:
        original = f.read()

    # ── Relative path and output path ────────────────────────────────────────
    rel = os.path.relpath(aidl_path, input_root)             # android/hardware/.../Foo.aidl
    rel_h = rel.replace(".aidl", ".h")                       # android/hardware/.../Foo.h
    rel_h_with_prefix = os.path.join("aidl", rel_h)          # aidl/android/hardware/.../Foo.h
    out_path = os.path.join(output_root, rel_h_with_prefix)

    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    # ── Parse: work on comment-stripped copy for structure, keep original for docs ──
    text = strip_comments_for_parsing(original)

    # Package
    pkg_m = re.search(r"package\s+([\w.]+)\s*;", text)
    if not pkg_m:
        print(f"  SKIP (no package): {rel}", file=sys.stderr)
        return False
    package = pkg_m.group(1)
    namespaces = package_to_namespace(package)

    # Imports — build a map of simple_name -> fully_qualified for type resolution
    imports = re.findall(r"import\s+([\w.]+)\s*;", text)
    import_map = {fq.split(".")[-1]: fq for fq in imports}

    # ── Determine construct type ──────────────────────────────────────────────
    enum_m = re.search(
        r"(?:@Backing\s*\(\s*type\s*=\s*\"(\w+)\"\s*\)\s*)?enum\s+(\w+)\s*\{",
        text
    )
    parcelable_m = re.search(r"parcelable\s+(\w+)\s*\{", text)

    # ── Guard and includes ────────────────────────────────────────────────────
    guard = make_include_guard(rel_h_with_prefix)

    lines = []
    lines.append(f"// Auto-generated from {rel} by generate_aidl_headers.py")
    lines.append(f"// DO NOT EDIT — regenerate by running scripts/generate_aidl_headers.py")
    lines.append(f"")
    lines.append(f"#pragma once")
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append(f"")
    lines.append(f"#include <cstdint>")
    lines.append(f"#include <string>")
    lines.append(f"#include <type_traits>")
    lines.append(f"#include <vector>")
    lines.append(f"")

    for imp in imports:
        lines.append(f"#include <{import_to_include(imp)}>")
    if imports:
        lines.append(f"")

    # Open namespaces
    lines.append(f"namespace aidl {{")
    for ns in namespaces:
        lines.append(f"namespace {ns} {{")
    lines.append(f"")

    # ── Enum ──────────────────────────────────────────────────────────────────
    if enum_m:
        # Re-search on original for backing type (annotations stripped in text)
        backing_m = re.search(r'@Backing\s*\(\s*type\s*=\s*"(\w+)"\s*\)', original)
        backing = backing_m.group(1) if backing_m else "int"
        cpp_backing = BACKING_TYPE_MAP.get(backing, "int32_t")

        enum_name = enum_m.group(2)

        # Extract enum body from original for comment preservation
        brace_pos = original.find("{", original.find(f"enum {enum_name}"))
        if brace_pos == -1:
            print(f"  ERROR: could not find enum body in {rel}", file=sys.stderr)
            return False
        body, _ = extract_block(original, brace_pos)

        values = parse_enum_values(body)

        # Upgrade backing type to unsigned if any value exceeds signed range
        # e.g. 0xF0000000 overflows int32_t but fits uint32_t
        SIGNED_MAX = {"int8_t": 127, "int16_t": 32767, "int32_t": 2147483647, "int64_t": 9223372036854775807}
        UNSIGNED_UPGRADE = {"int8_t": "uint8_t", "int16_t": "uint16_t", "int32_t": "uint32_t", "int64_t": "uint64_t"}
        if cpp_backing in SIGNED_MAX:
            for vname, vval, _ in values:
                if vval is not None:
                    try:
                        numeric = int(vval.strip(), 0)
                        if numeric > SIGNED_MAX[cpp_backing]:
                            cpp_backing = UNSIGNED_UPGRADE[cpp_backing]
                            break
                    except (ValueError, TypeError):
                        pass

        lines.append(f"enum class {enum_name} : {cpp_backing} {{")
        for name, value, doc in values:
            if doc:
                # Condense multiline doc to single line comment
                clean_doc = re.sub(r"\s+", " ", doc).strip()
                clean_doc = re.sub(r"/\*+\s*|\s*\*+/|\s*\*\s*", " ", clean_doc).strip()
                lines.append(f"    // {clean_doc}")
            if value is not None:
                lines.append(f"    {name} = {value},")
            else:
                lines.append(f"    {name},")
        lines.append(f"}};")
        lines.append(f"")

        # Bitwise operators for bitmask enums (always emit — harmless if not bitmask)
        lines.append(f"inline {enum_name} operator|({enum_name} a, {enum_name} b) {{")
        lines.append(f"    return static_cast<{enum_name}>(")
        lines.append(f"        static_cast<{cpp_backing}>(a) | static_cast<{cpp_backing}>(b));")
        lines.append(f"}}")
        lines.append(f"inline {enum_name} operator&({enum_name} a, {enum_name} b) {{")
        lines.append(f"    return static_cast<{enum_name}>(")
        lines.append(f"        static_cast<{cpp_backing}>(a) & static_cast<{cpp_backing}>(b));")
        lines.append(f"}}")
        lines.append(f"")

        # toString() — AIDL-generated debug helper, returns enum name as string
        # Deduplicate switch cases — AIDL allows multiple names with same value.
        # For enums with computed/expression values (e.g. VehicleProperty),
        # fall back to numeric string to avoid unevaluatable duplicate detection.
        def is_simple_literal(v):
            if v is None:
                return False
            try:
                int(v.strip(), 0)
                return True
            except (ValueError, TypeError):
                return False

        all_simple = all(is_simple_literal(v) for _, v, _ in values if v is not None)

        lines.append(f"inline std::string toString({enum_name} val) {{")
        if all_simple:
            lines.append(f"    switch (val) {{")
            seen_values = set()
            for vname, vval, _ in values:
                numeric_key = str(int(vval.strip(), 0)) if vval is not None else vname
                if numeric_key in seen_values:
                    continue
                seen_values.add(numeric_key)
                lines.append(f"        case {enum_name}::{vname}: return \"{vname}\";")
            lines.append(f"        default: return std::to_string(static_cast<{cpp_backing}>(val));")
            lines.append(f"    }}")
        else:
            # Complex/computed enum values — numeric fallback only
            lines.append(f"    return std::to_string(static_cast<{cpp_backing}>(val));")
        lines.append(f"}}")
        lines.append(f"")

    # ── Parcelable ────────────────────────────────────────────────────────────
    elif parcelable_m:
        parcelable_name = parcelable_m.group(1)

        brace_pos = original.find("{", original.find(f"parcelable {parcelable_name}"))
        if brace_pos == -1:
            print(f"  ERROR: could not find parcelable body in {rel}", file=sys.stderr)
            return False
        body, _ = extract_block(original, brace_pos)

        fields = parse_parcelable_fields(body, import_map)

        lines.append(f"struct {parcelable_name} {{")
        for cpp_type, name, default, doc in fields:
            if doc:
                clean_doc = re.sub(r"\s+", " ", doc).strip()
                clean_doc = re.sub(r"/\*+\s*|\s*\*+/|\s*\*\s*", " ", clean_doc).strip()
                lines.append(f"    // {clean_doc}")
            if cpp_type.startswith("static constexpr"):
                # const fields — static constexpr, no instance default init
                lines.append(f"    {cpp_type} {name} = {default};")
            elif default is not None:
                lines.append(f"    {cpp_type} {name} = {default};")
            else:
                lines.append(f"    {cpp_type} {name}{{}};")
        # toString() — debug helper for parcelable structs
        lines.append(f"    std::string toString() const {{")
        lines.append(f"        return \"{parcelable_name}{{}}\" ;")
        lines.append(f"    }}")

        # Emit comparison operators for parcelable structs
        lines.append(f"}};")
        lines.append(f"")

        # operator== and operator!= — needed by VehiclePropertyStore and others
        # Exclude static constexpr constants from comparison — only compare instance fields
        field_names = [name for cpp_type, name, _, _ in fields
                       if not cpp_type.startswith("static constexpr")]
        if field_names:
            cmp_expr = " && ".join(f"lhs.{n} == rhs.{n}" for n in field_names)
        else:
            cmp_expr = "true"
        lines.append(f"inline bool operator==(const {parcelable_name}& lhs, const {parcelable_name}& rhs) {{")
        lines.append(f"    return {cmp_expr};")
        lines.append(f"}}")
        lines.append(f"inline bool operator!=(const {parcelable_name}& lhs, const {parcelable_name}& rhs) {{")
        lines.append(f"    return !(lhs == rhs);")
        lines.append(f"}}")
        lines.append(f"")

    else:
        # Interface or unknown construct — emit empty placeholder
        lines.append(f"// NOTE: This file contains an interface or unsupported construct.")
        lines.append(f"// It was not converted — add manually if needed.")
        lines.append(f"")

    # Close namespaces
    for ns in reversed(namespaces):
        lines.append(f"}} // namespace {ns}")
    lines.append(f"}} // namespace aidl")
    lines.append(f"")
    lines.append(f"#endif // {guard}")
    lines.append(f"")

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

    return True


def main():
    parser = argparse.ArgumentParser(
        description="Generate C++ headers from AIDL property files"
    )
    parser.add_argument(
        "--input", required=True,
        help="Root of aidl_property/ directory"
    )
    parser.add_argument(
        "--output", required=True,
        help="Output root directory (headers written under <output>/aidl/...)"
    )
    args = parser.parse_args()

    input_root = os.path.abspath(args.input)
    output_root = os.path.abspath(args.output)

    if not os.path.isdir(input_root):
        print(f"ERROR: input directory not found: {input_root}", file=sys.stderr)
        sys.exit(1)

    aidl_files = []
    for root, _, files in os.walk(input_root):
        for f in files:
            if f.endswith(".aidl"):
                aidl_files.append(os.path.join(root, f))

    aidl_files.sort()
    print(f"Found {len(aidl_files)} AIDL files in {input_root}")

    ok = 0
    fail = 0
    for aidl_file in aidl_files:
        rel = os.path.relpath(aidl_file, input_root)
        try:
            if generate_header(aidl_file, input_root, output_root):
                print(f"  OK  {rel}")
                ok += 1
            else:
                fail += 1
        except Exception as e:
            print(f"  ERR {rel}: {e}", file=sys.stderr)
            fail += 1

    print(f"\nDone: {ok} generated, {fail} skipped/failed")
    sys.exit(0 if fail == 0 else 1)


if __name__ == "__main__":
    main()
