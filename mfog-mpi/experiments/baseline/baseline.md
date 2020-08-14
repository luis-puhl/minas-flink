# Baseline

## Parameter F

Optimize `f` param for total unknowns in stream.
F is used in `radius = mean + f * stdDev`.

| Summary           | Minas                 | Mfog                  |
|---                | ---:                  | ---:                  |
| Unknowns          |   11980 (  1.801214%) |    8661 (  1.313407%) |

| F param           | Unknowns  |
|---:               | ---:      |
| 1.600000e-01      |   28101   |
| 1.800000e-01      |   23830   |
| 1.900000e-01      |   10771   |
| 1.850000e-01      |   23468   |
| 1.895000e-01      |   11226   |
| 1.892500e-01      |   12047   |
| 1.894000e-01      |   11397   |
| 1.893000e-01      |   11892   |

So, `0.1893` is close enough.

This param controls _match vs unk_ and reflects in novelty detection which in
turn gives more clusters and increased hit rate.

## CluStream

Using CluStream as the clustering step can be faster with params `q = k`
but with `q = 300` and `k = 100` it was slower.

| q     | k     | step      | seconds       | labels    | Hits      |
|-------|-------|-----------|--------------:|----------:|----------:|
| 100   | 100   | training  | 1.894700e+01  |           |           |
| 100   | 100   | training  | 1.905754e+01  |           |           |
| 100   | 100   | online    | 1.225185e+01  |           |           |
| 100   | 100   | online    | 1.265624e+01  | 12        | .32002470 |
| 200   | 100   | training  | 7.061478e+01  |           |           |
| 200   | 100   | online    | 3.200653e+01  | 04        | .30764175 |
| 300   | 100   | training  | 1.554176e+02  |           |           |
| 300   | 100   | online    | 6.059204e+01  | 29        | .31826689 |

Other configs used:

```log
./bin/minas
k = 100
dim = 22
precision = 1.000000e-08
radiusF = 1.000000e-01
minExamplesPerCluster = 20
noveltyF = 1.000000
useCluStream = 1
cluStream_q_maxMicroClusters = 200
cluStream_time_threshold_delta_δ = 20.000000
```

## Square Distance

Difference between running `sqrt()` once per cluster versus once per
lookup nearest function. Total average per function in 2 runs.

| function            | sqrt      | sqr       | ratio     |
| --------            | ---:      | ---:      | ---:      |
| training            | 6.04E+01  | 6.04E+01  | 100.06%   |
| kMeans              | 2.71E+00  | 2.84E+00  | 104.89%   |
| noveltyDetection    | 4.03E-01  | 4.07E-01  | 101.16%   |
| minasOnline         | 4.29E+01  | 4.09E+01  | 95.40%    |

~~Maybe in ARM with less FLOPS this will make a bigger difference.~~
Found out there was a mistake. Redo experiment.

| function          | common        | fast          | ratio         |
| --------          | ---:          | ---:          | ---:          |
| training          | 6.850990e+01  | 3.009816e+01  | 0.439325703   |
| noveltyDetection  | 2.555810e-01  | -             | -             |
| minasOnline       | 1.187446e+01  | 1.674746e+01  | 1.410376556   |

## Reinterpretation

Optimize `f` param for total unknowns in stream.
F is used in `radius = f * stdDev`. As the center is the mean.
Reprocessing of unknowns is now done with new clusters only and by max distance.

| MinEx | Radius F. | Novelty F.| Unknowns (rep)    | n-Labels  | Hits          | Online Time   |
| ---:  |---:       | ---:      | ---:              | ---:      | ---:          | ---:          |
| 20    | 2.5e-01   | 1.4e+00   | 12844             | 15        | 31.134635%    | 1.674746e+01  |
| 20    | 1.0e-01   | 1.0e+00   | 72830             | 21        | 28.876600%    | 1.796509e+01  |
| 20    | 2.0e-01   | 1.0e+00   | 17057             | 23        | 37.501195%    | 9.457995e+00  |
| 20    | 2.0e-01   | 5.0e-01   | 17057 (16023)     | 44        | 38.188445%    | 9.112339e+00  |
| 20    | 1.5e-01   | 5.0e-01   | 32575 (31459)     | 41        | 37.689731%    | 1.269850e+01  |
| 20    | 1.5e-01   | 5.0e-01   | 33117 (32353)     | 52 (1)    | 33.926306%    | 1.369894e+01  |
| 20    | 1.0e-01   | 5.0e-01   | 71761 (70516)     | 67 (2)    | 231524 (31%)  | 1.864930e+01  |
| 20    | 1.0e-01   | 5.0e-01   | 72830 (71289)     | 72        | 230227 (31%)  | 2.035667e+01  |
| 20    | 1.0e-01   | 7.5e-01   | 72830 (71289)     | 37        | 210774 (29%)  | 1.939271e+01  |
| 20    | 9.0e-02   | 7.5e-01   | 90545 (89116)     | 27        | 207371 (27%)  | 2.131929e+01  |
| 20    | 9.0e-02   | 5.0e-01   | 90545 (89116)     | 60        | 220128 (29%)  | 2.140354e+01  |
| 20    | 2.0e-01   | 5.0e-01   | 17057 (16023)     | 44        | 255664 (38%)  | 9.357076e+00  |
| 40    | 2.0e-01   | 5.0e-01   | 11980 (11650)     | 17        | 207669 (31%)  | 8.450133e+00  |
| 40    | 1.0e-01   | 5.0e-01   | 84446 (80846)     | 16        | 221456 (30%)  | 1.573907e+01  |
| 40    | 1.0e-01   | 7.5e-01   | 84446 (80846)     | 10        | 220968 (30%)  | 1.596614e+01  |
| 40    | 2.0e-01   | 7.5e-01   | 17366 (15158)     | 7         | 235801 (35%)  | 8.299776e+00  |
| 40    | 2.0e-01   | 2.5e-01   | 17366 (15158)     | 55        | 248809 (37%)  | 8.060750e+00  |
| 40    | 1.0e-01   | 2.5e-01   | 84446 (80846)     | 54        | 238041 (32%)  | 1.560420e+01  |
| 50    | 1.0e-01   | 2.5e-01   | 87743 (84491)     | 47        | 242919 (32%)  | 1.732753e+01  |
| 50    | 1.0e-01   | 2.5e-01   | 87743 (87085)     | 48 (3)    | 244814 (33%)  | 1.728156e+01  |
| 50    | 9.0e-02   | 2.5e-01   | 91470 (91109)     | 38        | 226427 (30%)  | 1.841067e+01  |
| 50    | 9.0e-02   | 2.0e-01   | 91470 (91109)     | 64        | 278881 (37%)  | 1.776572e+01  |
| 50    | 5.0e-02   | 2.0e-01   | 186433 (185853)   | 102       | **294306** (35%)| 3.056771e+01  |
| 50    | 5.0e-02   | 2.5e-01   | 186433 (185853)   | 74        | 273008 (32%)  | 3.222774e+01  |
| 25    | 5.0e-02   | 2.0e-01   | 175904 (175642)   | 204       | 279672 (33%)  | 3.410292e+01  |
| 100   | 5.0e-02   | 2.0e-01   | 167496 (166759)   | 49        | 242318 (29%)  | 2.980112e+01  |
| 20    | 5.0e-02   | 2.5e-01   | 172866 (172572)   | 189       | 254385 (30%)  | 4.099720e+01  |
| 20    | 5.0e-02   | 4.0e-01   | 172866 (172572)   | 127       | 242770 (29%)  | 4.111075e+01  |

1. Reclassification using full model and max distance.
2. Wrap around of labels and elipses of confusion matrix.
3. Added same clusters and maxDist modification to final flush.

### Use floating cluster. Meaning the summary is updated for each match

```latex
Cluster + Delta
(LS, SS, N) + (LS, SS, N)
```

| MinEx  | Radius F. | Novelty F.| Unknowns (rep)    | n-Labels  | Hits          | Online Time   |
| ---:   |---:       | ---:      | ---:              | ---:      | ---:          | ---:          |
| 50 (1) | 5.0e-02   | 2.0e-01   | 186433 (185853)   | 102       | 294306 (35%)  | 3.056771e+01  |
| 50     | 5.0e-02   | 2.0e-01   | 614597 (613930)   | 95        | 295251 (23%)  | 6.490573e+01  |
| 20 (1) | 1.0e-01   | 2.0e+00   | 72830 (72587)     | 8         | 208056 (28%)  | 1.863111e+01  |
| 20     | 1.0e-01   | 2.0e+00   | 622312 (622066)   | 24        | 213911 (16%)  | 8.612032e+01  |

1. Sans moving cluster.

Moving cluster, better but only 10k more matches.

### Distribuição

Sempre que um novo delta chegar e propagar quando dN for relevante.
Enviar o delta para nuvem.
