# Baseline

## Parameter F

Optimize `f` param for total unknowns in stream.
F is used in `radius = mean + f * stdDev`.

| Summary           | Minas                 | Mfog      |
|---                | ---:                   | ---:|
| Unknowns          |   11980 (  1.801214%) |    8661 (  1.313407%)             |

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
