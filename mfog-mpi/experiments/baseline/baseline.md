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

| Radius F.     | Novelty F.    | Unknowns  | n-Labels  | Hits          | Online Time   |
|---:           |---:           | ---:      | ---:      | ---:          | ---:          |
<!-- | (ref) 2.0     | (ref) 1.0     | 11980 (r) | 14 (r)    |               |
| 5.000000e-01  | 1.000000e+00  |  7005     | 11        |               |
| 3.500000e-01  | 1.000000e+00  | 31883     | 6         |               |
| 4.000000e-01  | 1.000000e+00  | 13840     | 12        |               |
| 4.500000e-01  | 1.000000e+00  | 13537     | 12        | 5.516883e+01  |
| 4.750000e-01  | 1.000000e+00  |  9317     | 11        | 3.285049e+01  |
| 4.625000e-01  | 1.000000e+00  | 12387     | 10        | 4.449486e+01  |
| 4.700000e-01  | 1.000000e+00  | 10850     | 11        | 3.484519e+01  |
| 4.600000e-01  | 1.000000e+00  | 11503     | 10        | 5.027491e+01  | -->
| 2.500000e-01  | 1.400000e+00  | 12844     | 15        | 31.134635%    | 1.674746e+01  |

Use floating cluster. Meaning the summary is updated for each match.

```latex
Cluster + Delta
(LS, SS, N) + (LS, SS, N)
```

Sempre que um novo delta chegar e propagar quando dN for relevante.
Enviar o delta para nuvem.
