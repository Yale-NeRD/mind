#!/usr/bin/env python

import argparse

# This script gets two arguments: a list of tests, and a list of unfiltered tests.

def print_filtered(tests, unfiltered_tests):
    before_filter = list(map(lambda x : x.replace(".wrap",""), unfiltered_tests.split()))
    after_filter = list(map(lambda x : x.replace(".wrap",""), tests.split()))
    filtered_tests = list(filter(lambda x: x not in after_filter, before_filter))
    print("{} filtered tests: {}".format(len(filtered_tests), filtered_tests))
    
def parse_argv():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="Script to print information about filtered tests.\n")

    parser.add_argument('--tests',
                        help='The list of test to execute')

    parser.add_argument('--unfiltered_tests',
                        help='The list of test to execute prior to filtering')

    args = parser.parse_args()

    if not args.tests: parser.error("Parameter {} is required".format("--tests"))
    if not args.unfiltered_tests: parser.error("Parameter {} is required".format("--unfiltered_tests"))
    
    return args

def main():
    args = parse_argv()
    print_filtered(args.tests, args.unfiltered_tests)
    return 0

    
if __name__ == "__main__":
    exit(main())
