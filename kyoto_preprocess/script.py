#!/usr/bin/env python
# -*- coding: utf-8 -*-
import argparse
import sys
import random
import math
import copy
# import separate_files


def convert_to_float(l_split: list, classes: list) -> list:
    for i in range(len(l_split)):
        if i not in classes:
            try:
                aux = float(l_split[i])
            except ValueError:
                aux = l_split[i]
            l_split[i] = aux
    return l_split


def write_to_file_ordered(d_rec: dict, s: str, flag: bool) -> None:
    d = copy.deepcopy(d_rec)
    if flag:
        if not s:
            s = "outputFromDict"
        else:
            print(s)
        # print a quick summary of which keys will be present in the final file
        instances = 0
        print("(key), # of instances\n---------------------")
        for w in sorted(d, key=d.get, reverse=True):
            print(str(w) + ", " + str(d[w][0]))
            instances += d[w][0]
        print("Total instances " + str(instances) + "\n\n\n")
    # write to file
    out_file = open(s, "w")
    # while d is not empty
    while d:
        # control vars
        smallest = int(sys.maxsize)
        key = ""
        # check the first element from each key, if it marches index pop it
        for k in d.keys():
            if d[k][1][0][0] <= smallest:
                smallest = d[k][1][0][0]
                key = k
        aux = d[key][1].pop(0)
        d[key][0] -= 1
        if d[key][0] == 0:
            del d[key]
        out_file.write(','.join(map(str, aux[1])) + "\n")
        # print(aux)
    pass


def move_classes_to_the_end(classes: list, d: dict) -> dict:
    # receives the index of the class(es) and move that to the end of the instance
    for k in d.keys():
        for index, inst in d[k][1]:
            for i in range(len(classes)):
                inst.append(inst.pop(classes[i] - i))
    return d


def any_att_moved_before(index: int, moved: list) -> int:
    # checks if any attribute with index smaller than received index has been moved before
    # if yes, counts and returns this number
    a = 0
    for i in moved:
        if i < index:
            a += 1
    return a


def remove_attributes(ids: list, moved: list, d: dict) -> dict:
    # receives the index(es) of the attribute(s) to be removed and the list of attributes that have been moved
    for k in d.keys():
        for index, inst in d[k][1]:
            for i in range(len(ids)):
                # checks if there has been changes in attributes with smaller indexes and adjusts index accordingly
                attributes_moved = any_att_moved_before(ids[i], moved)
                inst.pop(ids[i] - i - attributes_moved)
    return d


def normalize_fields_0_to_1(fields: list, d: dict) -> dict:
    # in order to normalize, we'll have to iterate 2 times
    # FIRST for to check highest and lowest values for each field
    max_field = [0] * len(fields)
    min_field = [int(sys.maxsize)] * len(fields)
    for k in d.keys():
        for index, l in d[k][1]:
            for i in range(len(fields)):
                if fields[i] == 0:
                    if l[fields[i]] > 25:
                        l[fields[i]] = 25
                if fields[i] == 2:
                    if l[fields[i]] > 2250:
                        l[fields[i]] = 2250
                if fields[i] == 3:
                    if l[fields[i]] > 2900:
                        l[fields[i]] = 2900
                try:
                    if l[fields[i]] > max_field[i]:
                        max_field[i] = l[fields[i]]
                    if l[fields[i]] < min_field[i]:
                        min_field[i] = l[fields[i]]
                except TypeError:
                    print("Can't normalize a non-numeric attribute. Removing " + str(fields[i]) + " from the list of attributes to normalize.")
                    fields.remove(fields[i])
    # after this for we already have 2 lists containing the maximum and minimum values on the specified fields
    # time to update.. SECOND for
    print(max_field)
    for k in d.keys():
        for l in range(len(d[k][1])):
            for i in range(len(fields)):
                # normalize number and update on dictionary
                d[k][1][l][1][fields[i]] = (d[k][1][l][1][fields[i]] - min_field[i]) / (max_field[i] - min_field[i])
    return d


def binarize_nominal_fields_1_to_n(fields: list, d: dict) -> dict:
    # in order to normalize, we'll have to iterate 2 times
    # FIRST for to check the number of different classes in each attribute
    d_aux = {}
    for key in fields:
        d_aux[key] = []
    for k in d.keys():
        for index, l in d[k][1]:
            for i in range(len(fields)):
                if l[fields[i]].strip("\n") not in d_aux[fields[i]]:
                    d_aux[fields[i]].append(l[fields[i]].strip("\n"))
    # after this for we already have 1 dictionary containing all the nominal values for each specified field
    # time to create new attributes.. SECOND for
    for k in d.keys():
        for index, l in d[k][1]:
            for i in range(len(fields)):
                att = fields[i]
                # fields[i] is the key to new dict
                for v in d_aux[att]:
                    if l[att].strip("\n") == v:
                        # add new field with 1
                        l.append('1')
                    else:
                        # add new field with 0
                        l.append('0')
    return d


def sep_file(percentage: float, classes: list, d: dict) -> (dict, dict):
    # count the number of instances and determine, based on parameter, how many will be used for offline phase
    total_instances = 0
    for k in d.keys():
        total_instances += d[k][0]
    # make sure the offline instances number is close to percentage and multiple of 2000
    offline_instances = int(int((total_instances * percentage) / 2000) * 2000)
    print("OFFLINE INSTANCES: " + str(offline_instances))
    print("TOTAL_INSTANCES: " + str(total_instances))
    print("PERCENTAGE: " + str(percentage))
    # initialize random and calculate the total range (examples from each class)
    random.seed()
    my_range = int(math.floor(offline_instances / len(classes)))
    print("OFFLINE: " + str(offline_instances) + " LEN: " + str(len(classes)) + " MY_RANGE: " + str(my_range))
    # initialize dict, replicas on classes means more instances of that class
    offline = {}
    for c in classes:
        offline[c] = [0, []]
    # populate online dict
    for count in range(my_range):
        for i in classes:
            # gets a random instance in that class list
            r_number = random.randint(0, len(d[i][1]) - 1)
            # remove from online dict
            inst = d[i][1].pop(r_number)
            d[i][0] -= 1
            # add on offline dict
            offline[i][0] += 1
            offline[i][1].append(inst)
    return d, offline


def main(file, classes, threshold, normalize, binarize, remove, separate, percentage, keys, suffix):
    d_combined = {}
    in_file = open(file, "r")
    index = 0
    for l in in_file.readlines():
        l_split = l.split("\t")
        length = len(l_split)
        maximum = max(sorted(remove + classes + binarize + normalize))
        # end program if user wants to move/normalize/binarize or the class is an index out of range
        if maximum > length:
            sys.exit("One of the indexes passed as argument is bigger than the number of fields.")
        index += 1
        ids_split = l_split[14].split(",")
        l_split = convert_to_float(l_split, classes)
        # ids_detection will be used to compound the key for dict
        ids_detection = ""
        # if has IDS detection code, take out the ( ) for each code and generate the key
        if ids_split[0] != "0":
            for i in ids_split:
                ids_detection += i[:-3]
                # if it's not the last element, add semi colon
                if ids_split.index(i) != (len(ids_split) - 1):
                    ids_detection += ","
            # outside the for, ids_detection is the first element of the combined key for the dict
            l_split[14] = ids_detection
        # if it's normal traffic, generate the key with 0
        else:
            l_split[14] = ids_split[0]
        # generate key with ids_detection and flag attributes
        key = l_split[14], l_split[17]
        # now that we have the key ready, we should add/insert to dict
        if key in d_combined:
            d_combined[key][0] += 1
            d_combined[key][1].append([index, l_split])
        else:
            d_combined[key] = [1, [[index, l_split]]]
    in_file.close()
    # after reading the whole file and creating the dict to store things
    # it is time to remove keys that appeared in less than <wanted number> instances
    for k in list(d_combined.keys()):
        if d_combined[k][0] < threshold:
            del d_combined[k]
    # get filename for future use
    filename = file.split('/')[-1]
    # normalize numeric
    if normalize:
        d_combined = normalize_fields_0_to_1(normalize, d_combined)
    # binarize nominal, move classes to end and remove old nominal attributes
    if binarize:
        d_combined = binarize_nominal_fields_1_to_n(binarize, d_combined)
        filename += "_binarized"
    # move classes to the end
    d_combined = move_classes_to_the_end(classes, d_combined)
    # d_combined = remove_attributes(binarize, classes, d_combined)
    # remove remaining attributes the user wanted
    if remove:
        d_combined = remove_attributes(sorted(remove + binarize), classes, d_combined)
    # Join class attributes in only one
    for k in d_combined.keys():
        for index, inst in d_combined[k][1]:
            # print(inst)
            aux = inst.pop()
            if aux == "-1":
                aux = "A"
            else:
                aux = "N"
            if len(classes) > 1:
                inst[-1] += "_" + aux
            else:
                inst.append(aux)
            pass
        pass
        # write dictionary entries to file
    # write_to_file_ordered(d_combined, filename + "_all", True)
    # creating 2 files for online/offline training
    if separate:
        # d_online, d_offline = separate_files.separate_file(float(percentage / 100.0), keys, d_combined)
        # d_online, d_offline = separate_files.separate_file(percentage, keys, d_combined)
        d_online, d_offline = sep_file(percentage, keys, d_combined)
        write_to_file_ordered(d_online, filename + "_online" + "_" + suffix, True)
        write_to_file_ordered(d_offline, filename + "_offline" + "_" + suffix, True)
    else:
        write_to_file_ordered(d_combined, filename + "_all", True)
    pass


def my_tuple(s):
    try:
        ids, flag = map(str, s.split(","))
        return ids, flag
    except TypeError:
        raise argparse.ArgumentTypeError("erro nas tuplas")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Pre process the Kyoto database. This script gets the unmodified data and outputs a csv file where "
                                                 "only IDS detection and normal traffic are present. Additionally, it's possible to normalize, "
                                                 "binarize and remove attributes. It's also possible to remove instances belonging to classes that "
                                                 "have less than certain amount of occurrences.")
    parser.add_argument("file", help="input file")
    requiredNamed = parser.add_argument_group('required named arguments')
    requiredNamed.add_argument("-c", "--classes", nargs='+', type=int, help='Index(es) of class(es) of the file', required=True)
    parser.add_argument("-t", "--threshold", type=int, help="minimum number of instances to keep a key", default=1000)
    parser.add_argument("-n", "--normalize", nargs='+', type=int, help="normalize attributes from passed indexes from 0 to 1", default=[])
    parser.add_argument("-b", "--binarize", nargs='+', type=int, help="binarize attributes from passed indexes using 1-to-n", default=[])
    parser.add_argument("-r", "--remove", nargs='+', type=int, help="remove attributes from passed indexes", default=[])
    separate_file = parser.add_argument_group('separate file arguments')
    separate_file.add_argument("-s", "--separate", action='store_true', help='indicates whether files should be split in online/offline or not')
    separate_file.add_argument("-p", "--percentage", type=int, help="percentage of instances to use in offline phase, integer number",
                               required=('--separate' in sys.argv or '-s' in sys.argv), default=0)
    separate_file.add_argument("-k", nargs='+', type=my_tuple, default=[], required=('--separate' in sys.argv or '-s' in sys.argv))
    separate_file.add_argument("--suffix", type=str, default="", required=('--separate' in sys.argv or '-s' in sys.argv))
    args = parser.parse_args()
    # if not args.k:
    #     parser.error("there must be at least 1 class key to split")
    print(args.file, sorted(args.classes), args.threshold, sorted(args.normalize), sorted(args.binarize), sorted(args.remove), args.separate,
          args.percentage/100.0, args.k, args.suffix)
    main(args.file, sorted(args.classes), args.threshold, sorted(args.normalize), sorted(args.binarize), sorted(args.remove), args.separate,
         args.percentage/100.0, args.k, args.suffix)
    pass
