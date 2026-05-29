from __future__ import annotations

import argparse

from .pipeline import check_file, normal_form_of_global
from .pretty import show_term


def main() -> None:
    parser = argparse.ArgumentParser(description="TinyChecker reconstruction CLI")
    parser.add_argument("path", help="source file path")
    parser.add_argument("--nf", dest="nf_name", help="print the normal form of a global name")
    parser.add_argument(
        "--conv",
        choices=["greedy", "whnf", "whnfv2"],
        default="whnf",
        help="conversion checking strategy",
    )
    args = parser.parse_args()

    _, _, _, global_ctx = check_file(args.path, conv_strategy=args.conv)
    print("typecheck: OK")
    if args.nf_name:
        nf = normal_form_of_global(args.nf_name, global_ctx)
        print(f"nf {args.nf_name}: {show_term(nf)}")


if __name__ == "__main__":
    main()
