# Kyoto 2015 description

| Idx | Feature                           | Example |
| --- | --------                          | --------- |
| 1   | Duration                          | 10.330105 |
| 2   | Service                           | other |
| 3   | Source_bytes                      | 0 |
| 4   | Destination_bytes                 | 32155 |
| 5   | Count                             | 0 |
| 6   | Same_srv_rate                     | 0.00 |
| 7   | Serror_rate                       | 0.00 |
| 8   | Srv_serror_rate                   | 1.00 |
| 9   | Dst_host_count                    | 6 |
| 10  | Dst_host_srv_count                | 5 |
| 11  | Dst_host_same_src_port_rate       | 0.17 |
| 12  | Dst_host_serror_rate              | 0.00 |
| 13  | Dst_host_srv_serror_rate          | 0.00 |
| 14  | Flag                              | RSTOS0 |
| 15  | IDS_detection                     | 4060-1-8(1),1448-1-19(1),2418-1-10(1) |
| 16  | Malware_detection                 | 0 |
| 17  | Ashula_detection                  | 0 |
| 18  | Label                             | -1 |
| 19  | Source_IP_Address                 | fdbd:f115:35b2:6379:4975:4856:4132:1eaa |
| 20  | Source_Port_Number                | 62518 |
| 21  | Destination_IP_Address            | fdbd:f115:35b2:aba0:7dcf:2752:0f2c:15e1 |
| 22  | Destination_Port_Number           | 3389 |
| 23  | Start_Time                        | 00:00:00 |
| 24  | protocol                          | tcp |

## scripts

```sh

# wget http://www.takakura.com/Kyoto_data/new_data201704/2015/201512.zip

# cat datasets/Kyoto2016/header.tsv datasets/Kyoto2016/2015/12/* | awk -F '\t' '{print $15 }' |
# echo "1325-1-14(1),19559-1-6(1),6-128-2(1)" | sed -E 's;\([0-9]+\);;g' | sed -E 's;,;\n;g' |

# get field 15 (IDS_detection), drop alarm counter, split the many alarm codes
cat datasets/Kyoto2016/2015/12/* \
    | awk -F '\t' '{print $15 }' \
    | sed -E 's;\([0-9]+\);;g' | sed -E 's;,;\n;g' \
    | sort | uniq -c | sort -bgr \
    > datasets/Kyoto2016/IDS_detection.count
# sort, count each code, sort by count
```

| count   | label     | count | label      | count | label     |
| -----   | -----     | ----- | -----      | ----- | -----     |
| 7338541 | 0         |  6083 | 382-1-11   |  1008 | 4-138-1   |
|  182761 | 19187-3-7 |  4764 | 4060-1-8   |   548 | 17110-1-4 |
|   71535 | 21355-3-4 |  4645 | 21817-1-4  |   527 | 2339-1-8  |
|   62404 | 384-1-8   |  3345 | 7209-1-20  |   489 | 4-128-2   |
|   54328 | 6-128-2   |  3337 | 14782-1-21 |   414 | 404-1-14  |
|   42188 | 1917-1-15 |  3265 | 449-1-9    |   356 | 8710-1-10 |
|   34269 | 402-1-15  |  3149 | 3-133-2    |   356 | 17294-1-8 |
|   17296 | 28556-1-2 |  2617 | 22114-1-5  |   307 | 34464-1-1 |
|   13150 | 19559-1-6 |  2332 | 1616-1-16  |   297 | 26-133-2  |
|   11788 | 2049-1-8  |  1376 | 1417-1-17  |   288 | 399-1-9   |
|   11476 | 1325-1-14 |  1376 | 1411-1-19  |   284 | 366-1-11  |
|   10689 | 1448-1-19 |  1230 | 1418-1-18  |   282 | 368-1-10  |
|   10635 | 2418-1-10 |  1128 | 23039-3-3  |   279 | 373-1-10  |

| count | label      | count | label      | count | label      |
| ----- | -----      | ----- | -----      | ----- | -----      |
|   276 | 17110-1-3  |  20   | 257-1-17   |     2 | 9419-1-9   |
|   263 | 22113-1-5  |  20   | 16350-1-5  |     2 | 255-1-23   |
|   254 | 385-1-8    |  18   | 24304-1-2  |     2 | 2383-1-26  |
|   210 | 396-1-10   |  14   | 34-133-2   |     2 | 2382-1-25  |
|   140 | 21546-1-3  |  14   | 3397-1-17  |     2 | 1447-1-19  |
|   133 | 16282-1-4  |  14   | 24378-1-1  |     2 | 1421-1-18  |
|   131 | 15167-1-12 |  12   | 3967-1-15  |     2 | 1420-1-18  |
|   119 | 254-1-15   |  12   | 3626-1-8   |     2 | 12798-1-10 |
|    80 | 1867-1-6   |  11   | 2508-1-24  |     2 | 12710-1-4  |
|    77 | 10010-1-7  |  11   | 16167-1-11 |     2 | 12198-1-15 |
|    60 | 22115-1-5  |  10   | 20242-1-7  |     1 | 9421-1-11  |
|    54 | 17322-1-7  |  9    | 1280-1-17  |     1 | 579-1-16   |
|    52 | 483-1-10   |  6    | 13364-1-13 |     1 | 51-133-1   |
|    39 | 13719-1-5  |  5    | 2-138-1    |     1 | 42-133-2   |
|    32 | 567-1-17   |  5    | 21232-1-4  |     1 | 28069-1-1  |
|    28 | 648-1-18   |  5    | 17344-1-7  |     1 | 15860-1-13 |
|    28 | 18175-1-8  |  5    | 13162-1-12 |     1 | 12946-1-9  |
|    27 | 381-1-11   |  4    | 1390-1-15  |     1 | 12489-1-13 |
|    27 | 1394-1-16  |  3    | 9422-1-9   |     1 | 10-124-1   |
|    23 | 17317-1-11 |  3    | 2-133-2    |       |            |
|    22 | 15912-3-8  |  3    | 1444-1-9   |       |            |

```sh
# cat datasets/Kyoto2016/IDS_detection | while IFS= read -r line; do cat datasets/Kyoto2016/2015/12/* | grep "$line" > datasets/Kyoto2016/IDS_$line ; done

cat datasets/Kyoto2016/header.tsv | awk -F '\t' '{print $16 }' > datasets/Kyoto2016/dic/16-mlw.dic ;
cat datasets/Kyoto2016/full.tsv | awk -F '\t' '{print $16 }' | sort | uniq -c | sort -rgb > datasets/Kyoto2016/dic/16-mlw.dic &

# IDS_lbls='' ; cat datasets/Kyoto2016/IDS_detection | while IFS= read -r line; do IDS_lbls="$IDS_lbls|$line"; done
IDS_lbls='0|19187-3-7|21355-3-4|384-1-8|6-128-2|1917-1-15|402-1-15|28556-1-2|19559-1-6|2049-1-8|1325-1-14|1448-1-19|2418-1-10'

cat datasets/Kyoto2016/2015/12/* | awk -F '\t' "\$15 ~ /$IDS_lbls/ {print NR, \$0 }" > datasets/Kyoto2016/IDS_dataset

# 4 Label
# Indicates whether the session was attack or not;
#   ‘1’ means normal. 
#   ‘-1’ means known attack was observed in the session, and
#   ‘-2’ means unknown attack was observed in the session.
cat datasets/Kyoto2016/header.tsv datasets/Kyoto2016/IDS_dataset | awk -F '\t' '{print $18 }' | sort | uniq -c | sort -bgr
# 7556524 -1
#  293816 1
#     173 -2
#       1 #Label

for i in {1..24};
    do cat datasets/Kyoto2016/header.tsv datasets/Kyoto2016/IDS_dataset \
        | awk -F '\t' '{print $18 }' \
        | sort | uniq -c | sort -bgr \
        >> datasets/Kyoto2016/IDS_dataset.dic;
done;
```
