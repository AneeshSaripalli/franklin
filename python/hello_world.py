#!/usr/bin/env python3
"""
Franklin CFFI - Hello World Example
"""

import franklin


def main():
    print("=" * 60)
    print("Franklin CFFI - Hello World")
    print("=" * 60)

    print(f"\nFranklin version: {franklin.version()}")

    # Create interpreter
    print("\nCreating interpreter...")
    interp = franklin.Interpreter(dtype="int32")
    print(f"Interpreter created. Registered columns: {len(interp)}")

    # Create columns using generic interface
    print("\nCreating columns...")
    a = franklin.Column.create("int32", size=5, value=10)
    b = franklin.Column.create("int32", size=5, value=20)
    c = franklin.Column.create("int32", size=5, value=5)

    print(f"Column 'a': size={len(a)}, values={a.to_list()}")
    print(f"Column 'b': size={len(b)}, values={b.to_list()}")
    print(f"Column 'c': size={len(c)}, values={c.to_list()}")

    # Register columns
    print("\nRegistering columns...")
    interp.register("a", a)
    interp.register("b", b)
    interp.register("c", c)

    print(f"Registered columns: {len(interp)}")
    print(f"Has 'a': {interp.has_column('a')}")
    print(f"Has 'b': {interp.has_column('b')}")
    print(f"Has 'c': {interp.has_column('c')}")

    # Evaluate simple expression
    print("\n" + "-" * 60)
    print("Evaluating: 'a'")
    result = interp.eval("a")
    print(f"Result: size={len(result)}, values={result.to_list()}")

    print("\n" + "=" * 60)
    print("Demo complete!")
    print("=" * 60)

    try:
        print("\nAttempting: 'a + b'")
        result = interp.eval("a + b")
        print(f"Result: {result.to_list()}")
    except RuntimeError as e:
        print(f"Expected error: {e}")


if __name__ == "__main__":
    main()
