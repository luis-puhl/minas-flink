# Keep It Simple, Stupid

## Parallelism 1

16:29:10 INFO  Producer B  sending...  ...done Connection 1 102 items, 82 ms, 81292859 ns, 1 i/ms

16:29:18 INFO  Producer A  sending...  ...done Connection 1 700002 items, 8254 ms, 8253354900 ns, 84 i/ms

16:29:18 INFO  Consumer  disconnected Connection 1 700002 items, 8597 ms, 8596897205 ns, 81 i/ms

Program execution finished
Job with JobID 4ecd030bc238784a97193b7ca03b234b has finished.
Job Runtime: 9020 ms

## Parallelism 2

16:24:25 INFO  Producer B  sending...  ...done Connection 1 102 items, 85 ms, 84898485 ns, 1 i/ms
16:24:25 INFO  Producer B  sending...  ...done Connection 2 2 items, 27 ms, 26995178 ns, 0 i/ms

16:24:33 INFO  Producer A  sending...  ...done Connection 1 357261 items, 7375 ms, 7375122943 ns, 48 i/ms
16:24:33 INFO  Producer A  sending...  ...done Connection 2 342743 items, 7316 ms, 7315979725 ns, 46 i/ms

16:24:33 INFO  Consumer  disconnected Connection 2 342742 items, 5359 ms, 5358588076 ns, 63 i/ms
16:24:33 INFO  Consumer  disconnected Connection 1 357260 items, 7806 ms, 7806109477 ns, 45 i/ms

Program execution finished
Job with JobID 5906898a475fffaa3e82516ea501dd45 has finished.
Job Runtime: 8270 ms

## Parallelism 4

16:23:32 INFO  Producer B  sending...  ...done Connection 1 102 items, 97 ms, 97247193 ns, 1 i/ms
16:23:32 INFO  Producer B  sending...  ...done Connection 2 2 items, 31 ms, 30911090 ns, 0 i/ms
16:23:32 INFO  Producer B  sending...  ...done Connection 4 2 items, 31 ms, 30575600 ns, 0 i/ms
16:23:32 INFO  Producer B  sending...  ...done Connection 3 2 items, 31 ms, 31778719 ns, 0 i/ms

16:23:40 INFO  Producer A  sending...  ...done Connection 1 425478 items, 7906 ms, 7905263197 ns, 53 i/ms
16:23:40 INFO  Producer A  sending...  ...done Connection 3 2 items, 7830 ms, 7830169707 ns, 0 i/ms
16:23:40 INFO  Producer A  sending...  ...done Connection 2 274526 items, 7844 ms, 7844570649 ns, 34 i/ms
16:23:40 INFO  Producer A  sending...  ...done Connection 4 2 items, 7830 ms, 7830060779 ns, 0 i/ms

16:23:40 INFO  Consumer  disconnected Connection 1 425477 items, 7790 ms, 7789454098 ns, 54 i/ms
16:23:40 INFO  Consumer  disconnected Connection 2 274525 items, 3214 ms, 3213807873 ns, 85 i/ms

Program execution finished
Job with JobID 01bb5bb52ade647f8d851356ca22d370 has finished.
Job Runtime: 8613 ms

## Parallelism 8

16:22:47 INFO  Producer A  sending...  ...done Connection 2 700002 items, 9958 ms, 9957612845 ns, 70 i/ms
16:22:47 INFO  Producer A  sending...  ...done Connection 4 3 items, 9958 ms, 9957306985 ns, 0 i/ms
16:22:47 INFO  Producer A  sending...  ...done Connection 3 3 items, 9958 ms, 9958069208 ns, 0 i/ms
16:22:47 INFO  Producer A  sending...  ...done Connection 1 3 items, 10021 ms, 10020850999 ns, 0 i/ms
16:22:47 INFO  Producer A  sending...  ...done Connection 5 3 items, 9958 ms, 9957819747 ns, 0 i/ms
16:22:47 INFO  Producer A  sending...  ...done Connection 6 3 items, 9958 ms, 9957656793 ns, 0 i/ms
16:22:47 INFO  Producer A  sending...  ...done Connection 8 3 items, 9957 ms, 9956874885 ns, 0 i/ms
16:22:47 INFO  Producer A  sending...  ...done Connection 7 3 items, 9958 ms, 9957862177 ns, 0 i/ms

16:22:37 INFO  Producer B  sending...  ...done Connection 1 102 items, 89 ms, 88490659 ns, 1 i/ms
16:22:37 INFO  Producer B  sending...  ...done Connection 2 2 items, 35 ms, 34773396 ns, 0 i/ms
16:22:37 INFO  Producer B  sending...  ...done Connection 4 2 items, 35 ms, 34310476 ns, 0 i/ms
16:22:37 INFO  Producer B  sending...  ...done Connection 3 2 items, 35 ms, 35260538 ns, 0 i/ms
16:22:37 INFO  Producer B  sending...  ...done Connection 5 2 items, 19 ms, 18097261 ns, 0 i/ms
16:22:37 INFO  Producer B  sending...  ...done Connection 6 2 items, 17 ms, 16968893 ns, 0 i/ms
16:22:37 INFO  Producer B  sending...  ...done Connection 8 2 items, 16 ms, 15559254 ns, 0 i/ms
16:22:37 INFO  Producer B  sending...  ...done Connection 7 2 items, 19 ms, 18458473 ns, 0 i/ms

16:22:47 INFO  Consumer  disconnected Connection 1 700002 items, 9928 ms, 9927815658 ns, 70 i/ms

Program execution finished
Job with JobID fbb842576019d366a0af19d95778a3ef has finished.
Job Runtime: 10459 ms
