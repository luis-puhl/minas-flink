#!/usr/bin/env python
# -*- coding: utf-8 -*-
import argparse


def main(file, out):
    in_file = open(file, "r")
    out_file = open(out, "w")
    for l in in_file.readlines():
        l_split = l.split("\t")
        # if it's a known IDS attack OR normal traffic
        if (l_split[14] != "0" and l_split[17] == "-1") or (l_split[17] == "1"):
            out_file.write('\t'.join(map(str, l_split)))
    pass


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Pre process the Kyoto database. This script gets the unmodified data and outputs a csv file where "
                                                 "only IDS detection and normal traffic are present. Additionally, it's possible to normalize, "
                                                 "binarize and remove attributes. It's also possible to remove instances belonging to classes that "
                                                 "have less than certain amount of occurrences.")
    parser.add_argument("file", help="input file")
    parser.add_argument("out", help="output file")
    args = parser.parse_args()
    main(args.file, args.out)
    pass
