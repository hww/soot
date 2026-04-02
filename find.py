#!/usr/bin/env python3
"""
Advanced script to find duplicate method/type combinations in defmethod declarations.
Supports filtering and detailed reporting.
"""

import re
import sys
import argparse
from collections import defaultdict
from pathlib import Path

def extract_method_info(line):
    """Extract method name and this type from a defmethod line."""
    # Pattern handles:
    # - defmethod name ((this type) ...)
    # - defmethod-mips2c "name" type
    patterns = [
        # Standard defmethod
        r'defmethod\s+([\w!?-]+)\s+\(\(this\s+([\w!?-]+)\)',
        # defmethod-mips2c format
        r'defmethod-mips2c\s+"\(method\s+\d+\s+([\w!?-]+)\)"\s+\d+\s+([\w!?-]+)',
    ]
    
    for pattern in patterns:
        match = re.search(pattern, line)
        if match:
            method_name = match.group(1)
            this_type = match.group(2)
            return (method_name, this_type)
    return None

def parse_file(filepath):
    """Parse a single file and return list of method definitions."""
    results = []
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            for line_num, line in enumerate(f, 1):
                info = extract_method_info(line)
                if info:
                    method_name, this_type = info
                    results.append({
                        'file': str(filepath),
                        'line_num': line_num,
                        'line': line.strip()[:200],  # Truncate long lines
                        'method': method_name,
                        'type': this_type
                    })
    except Exception as e:
        print(f"Error reading {filepath}: {e}", file=sys.stderr)
    return results

def find_duplicates(entries):
    """Group entries by (method, type) and find duplicates."""
    groups = defaultdict(list)
    for entry in entries:
        key = (entry['method'], entry['type'])
        groups[key].append(entry)
    return {k: v for k, v in groups.items() if len(v) > 1}

def main():
    parser = argparse.ArgumentParser(
        description='Find duplicate method/type combinations in defmethod declarations'
    )
    parser.add_argument('files', nargs='*', help='Files to process')
    parser.add_argument('--method', '-m', help='Filter by method name')
    parser.add_argument('--type', '-t', help='Filter by type name')
    parser.add_argument('--summary', '-s', action='store_true', 
                       help='Show only summary, no details')
    parser.add_argument('--json', '-j', action='store_true',
                       help='Output in JSON format')
    
    args = parser.parse_args()
    
    # Get files from command line or stdin
    if args.files:
        files = args.files
    else:
        files = [line.strip() for line in sys.stdin if line.strip()]
    
    if not files:
        parser.print_help()
        sys.exit(1)
    
    all_entries = []
    for filepath in files:
        all_entries.extend(parse_file(filepath))
    
    # Filter by method/type if specified
    if args.method:
        all_entries = [e for e in all_entries if e['method'] == args.method]
    if args.type:
        all_entries = [e for e in all_entries if e['type'] == args.type]
    
    duplicates = find_duplicates(all_entries)
    
    if args.json:
        import json
        output = []
        for (method_name, this_type), entries in duplicates.items():
            output.append({
                'method': method_name,
                'type': this_type,
                'count': len(entries),
                'locations': [{'file': e['file'], 'line': e['line_num']} for e in entries]
            })
        print(json.dumps(output, indent=2))
        return
    
    if duplicates:
        if not args.summary:
            print("=" * 80)
            print("DUPLICATE METHOD DEFINITIONS FOUND")
            print("(same method name AND same (this type))")
            print("=" * 80)
            print()
        
        for (method_name, this_type), entries in sorted(duplicates.items()):
            if not args.summary:
                print(f"Method: {method_name}")
                print(f"Type: {this_type}")
                print(f"Found {len(entries)} times:")
                for entry in entries:
                    print(f"  - {entry['file']}:{entry['line_num']}")
                    print(f"    {entry['line']}")
                print()
            else:
                print(f"{method_name} ({this_type}): {len(entries)} times")
        
        if not args.summary:
            print(f"Total duplicate groups: {len(duplicates)}")
            print(f"Total files processed: {len(files)}")
            print(f"Total method definitions: {len(all_entries)}")
    else:
        print("No duplicate method/type combinations found.")
        print(f"Processed {len(files)} files, found {len(all_entries)} method definitions.")

if __name__ == "__main__":
    main()
