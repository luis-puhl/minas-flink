# TODO: __The List__

- 2020-06-08 [x] Fazer em branchs com Pull Request em https://github.com/HPCSys-Lab/minas-flink;
- 2020-06-08 [x] Otimizar Send/Recv com envio único usando MPI PACK;
- 2020-06-08 [x] Modularizar em funções menores (Read Model, Read Examples, [Send/Rcv]Model, [Send/Rcv]Examples);
- 2020-06-08 [x] Descontar File IO da contagem de tempo (ler tudo antes do MPI_Init); Refletir na implementação serial;
  - 2020-06-10 [x] Create a persistent log file with timing, speedup and the like;
- 2020-06-08 [x] Tempo no ROOT:
  - Enviar final de stream (EOF) para os workers, root espera todos workers responderem EOF, ao coletar todos contar o tempo;
  - done with ``` MPI_Barrier(MPI_COMM_WORLD); ```
- 2020-06-08 [ ] Nó exclusivo para recebimento de alertas e repasse de desconhecidos para retreinamento na nuvem;
  - Send takes time, can do many sends before a rcv. Maybe master should do this comms.
- 2020-06-10 [x] Do it all in files, bash pipes may be source of some locks;
- 2020-06-10 [ ] Integration test with small dataset:
  - [ ] Define dimension at runtime (that hardcoded 22 scanf...);
- 2020-06-10 [ ] Optimizations with MPI I-Send/I-Recv or buffer (micro-batch, time, back-pressure);
- 2020-06-12 [ ] Run on local<-->rpi, cloud<-->rpi;