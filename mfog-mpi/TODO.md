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
  - 2020-06-15 [ ] Processo exclusivo para alertas/ML/ND;
- 2020-06-10 [x] Do it all in files, bash pipes may be source of some locks;
~~- 2020-06-10 [ ] Integration test with small dataset:~~
  - [ ] Define dimension at runtime (that hardcoded 22 scanf...);
- 2020-06-10 [ ] Optimizations with MPI I-Send/I-Recv or buffer (micro-batch, time, back-pressure);
- 2020-06-12 [ ] Run on local<-->rpi, cloud<-->rpi;
  - 2020-07-01 [x] Run with heterogeneous cluster is not possible.
  - 2020-07-13 [ ] Socket comm: Cloud, multiple stream sources (opposing current fan-out model);
- 2020-06-15 [x] Detecção de novidades;
- 2020-06-15 [x] Homogenização de ambientes (USER, PATH, ...) ---->> Only on Rpi;
- 2020-06-15 [x] Vazão (Throughput) (70k/5s), Latência (1/vazão), speedup (T_seq/T_parallel);
- 2020-07-01 [x] Review evaluate. Use unix sort.
- 2020-07-13
  - Classificador sem ND/Training;
  - [x] Serial vs Paralelo (em C);
  - [ ] Finalizar treinamento e ND
    - [x] ~~Adicionar saídas no formato mfog (matches.csv e model.csv);~~
      - [x] Converter saída minas original para mfog (eval.py);
      - [ ] Header de model.csv
      - [ ] id cluster
      - [ ] cluster matches
    - [x] Desativar ND para comparação com a implementação atual;
    - [x] Comparar a implementação java (original) com a nova (mpi-c);
    - ~~[ ] Usar `bash` chamando `java -cp moa.jar moa.DoTask kmeans|clustream file`~~
    - [x] Implementar ND na versão serial (corretamente);
    - [ ] Implementar ND na versão MPI com um nó exclusivo (fan-in matches);
    - [ ] Implementar sockets para transmissão de exemplos, desconhecidos e ND
    - [x] Diff por imagem (clusters e exemplos?);
  - [ ] Valgrind memory leak analysis;
  - [ ] Constante de `dimension` em tempo de compilação (dataset constante);
- 2020-07-27
  - Modelos continuam não idênticos (porém bem mais próximos);
  - Implementado ND com alguns resultados favoráveis;
  - [x] Escrever um *teste de mesa* com `csv` de entrada,
        pequeno (K, d, n) (número de clusters, dimensões e exemplos),
        plot dos pontos e centroides; Existe diferença entre implementações mas
        mas não é visualizável https://docs.google.com/spreadsheets/d/1KGdMJmJBH0Xhb82ik6Do6Bz64d7jqrXn8H5ZVgTJPWc/edit#gid=0&range=G114:H123;
  - [ ] Options:
    - [ ] Implement sockets and cloud: new lib, new challenges;
    - [ ] Implement redis and cloud: new lib, new challenges;
    - [ ] ND on mpi, parallel k-means: not in the original architecture;
    - [ ] Clu-Stream: is faster, less error;
    - [ ] Run on rpi: will be very slow with ND (k-menas);
    - [ ] Plot ex distances, compare with og;
