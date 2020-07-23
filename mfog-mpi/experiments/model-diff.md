# Evaluate model creation

## Minas original versus new c/mpi implementation

### Minas

examples  (653457, 25)
matches   (653457, 9)
Confusion Matrix
Classes (act)       A       N assigned class     max
Labels (pred)                                       
-               19677     340              A   19677
N              427502  205938              A  427502
Total examples     653457
Total matches      653457
Hits               205938 ( 31.515157%)
Misses             447519 ( 68.484843%)
Hits + Misses      653457 (100.000000%)

### Mfog

examples  (653457, 25)
matches   (653457, 9)
Confusion Matrix
Classes (act)       A       N assigned class     max
Labels (pred)                                       
-               18417     342              A   18417
N              428762  205936              A  428762
Total examples     653457
Total matches      653457
Hits               205936 ( 31.514851%)
Misses             447521 ( 68.485149%)
Hits + Misses      653457 (100.000000%)

### Diff

            id j_clL    j_clR j_L       j_D c_clL     c_clR c_L       c_D
88          88     N  1.02634   -  1.105216     N  1.453797   N  1.105437
286        286     N  1.02634   -  1.097748     N  1.453797   N  1.097847
410        410     N  1.02634   -  1.083439     N  1.453797   N  1.083677
539        539     N  1.02634   -  1.085700     N  1.453797   N  1.086120
723        723     N  1.02634   -  1.086764     N  1.453797   N  1.086905
...        ...   ...      ...  ..       ...   ...       ...  ..       ...
652439  652439     N  1.02634   -  1.073879     N  1.453797   N  1.074178
652441  652441     N  1.02634   -  1.074051     N  1.453797   N  1.074350
652442  652442     N  1.02634   -  1.074049     N  1.453797   N  1.074348
652681  652681     N  1.02634   -  1.080975     N  1.453797   N  1.081161
653345  653345     N  1.02634   -  1.088815     N  1.453797   N  1.089072

[1326 rows x 9 columns]

## Minas original versus with revised k-means

### Minas

examples  (653457, 25)
matches   (653457, 9)
Confusion Matrix
Classes (act)       A       N assigned class     max
Labels (pred)                                       
-               19677     340              A   19677
N              427502  205938              A  427502
Total examples     653457
Total matches      653457
Hits               205938 ( 31.515157%)
Misses             447519 ( 68.484843%)
Hits + Misses      653457 (100.000000%)

### Minas with revised k-means

examples  (653457, 25)
matches   (653457, 9)
Confusion Matrix
Classes (act)       A       N assigned class     max
Labels (pred)                                       
-               19614     319              A   19614
N              427565  205959              A  427565
Total examples     653457
Total matches      653457
Hits               205959 ( 31.518371%)
Misses             447498 ( 68.481629%)
Hits + Misses      653457 (100.000000%)

### Diff

            id j_clL     j_clR j_L       j_D c_clL     c_clR c_L       c_D
1178      1178     N  0.503355   N  0.481878     N  0.468740   -  0.472784
2196      2196     N  0.786041   N  0.459109     N  0.398849   -  0.457938
15675    15675     N  0.082241   -  0.088899     N  0.099042   N  0.093019
15718    15718     N  0.132058   -  0.167713     N  0.278194   N  0.239603
15893    15893     N  0.685839   N  0.542879     N  0.537506   -  0.538066
...        ...   ...       ...  ..       ...   ...       ...  ..       ...
644933  644933     N  0.183655   -  0.309602     N  0.515985   N  0.312972
647565  647565     N  0.560968   N  0.324052     N  0.326379   -  0.329324
647954  647954     N  0.257275   -  0.260524     N  0.468740   N  0.253883
649337  649337     N  1.257792   N  0.771496     N  0.751851   -  0.765419
649364  649364     N  0.203591   -  0.251197     N  0.653072   N  0.250959

[552 rows x 9 columns]
