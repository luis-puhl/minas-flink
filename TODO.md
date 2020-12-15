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
      - [ ] Exemplos por socket;
      - [x] Desconhecidos por socket;
      - [ ] Modelo por socket [WIP];
    - [x] Diff por imagem (clusters e exemplos?);
  - [ ] Valgrind memory leak analysis;
  - [ ] Constante de `dimension` em tempo de compilação (dataset constante);
- 2020-07-27
  - Modelos continuam não idênticos (porém bem mais próximos);
  - Implementado ND com alguns resultados favoráveis;
  - [x] Escrever um *teste de mesa* com `csv` de entrada,
        pequeno (K, d, n) (número de clusters, dimensões e exemplos),
        plot dos pontos e centroides; Existe diferença entre implementações mas
        mas não é visualizável. [Planilha e visualização dos passos de k-means](<https://docs.google.com/spreadsheets/d/1KGdMJmJBH0Xhb82ik6Do6Bz64d7jqrXn8H5ZVgTJPWc/edit#gid=0&range=G114:H123>);
  - [ ] Options:
    - [ ] Implement sockets and cloud: new lib, new challenges;
    - [ ] Implement redis and cloud: new lib, new challenges;
    - [ ] ND on mpi, parallel k-means: not in the original architecture;
    - [ ] Clu-Stream: is faster, less error;
    - [ ] Run on rpi: will be very slow with ND (k-menas);
    - [ ] Plot ex distances, compare with og;
- 2020-08-03
  - Apresentação dos resultados de *teste de mesa* do kMeans.
  - Diferenças apresentadas no [e-mail](./notes.md#resumo-semana-30-de-2020) de resultados de classificação,
  em especial o número de etiquetas-novidade na [matrix de confusão](./notes.md#confusion-matrix).
  - [ ] Apresentar o meu entendimento do algoritmo original e os detalhes de implementação,
  em especial aquelas que diferem da implementação original.
  - [ ] Reavaliar arquitetura (sockets, redis, db, ND, cloud)

## Meeting 2020-08-05

Seguir o paper.\
Justificar escolhas que divergem.\
Implementação atual é razoável.\
Mudanças nas decisões são resultados válidos e devem ser publicados.

Comparação serial (baseline) paralelo apenas.

Estudo da distribuição do dataset em cada um dos nós da borda.

## Meeting 2020-12-01

- Deadline resultados 07/12/2021
- Deadline artigo 07/01/2021
- Deadline submissão 07/03/2021
- Deadline tese 07/05/2021
- Deadline defesa 07/07/2021

- Falha na implementação de Load Balancing, Network saving, etc.
  - Usar apenas Round Robin (Load balancing p/ trabalhos futuros);
  - Enviar Exemplo completo sampler->clasifier->detector;
- Roteiro de Experimentos:
  - Objetivos:
    - Teste de desempenho com foco em escabilidade forte (dataset tamanho fixo);
    - Avaliar impacto na qualidade de detecção (comparar sequencial vs paralelo);
  - Métricas:
    - Tempo total de execução, speedup, eficiência;
    - Taxa de acertos, taxa de desconhecidos;
  - Parâmetros e níveis:
    - Nó raiz: 1 nó, 1 thread `sampler`, 1 thread `detector`;
    - Nó folha: $1..N$ nós, 1 thread m_update, $1..2$ threads classifier;
  - Método:
    - [ ] Teste de escalabilidade (`mpirun -n [2..12]`);
    - [ ] Teste de qualidade (`evaluate.py`);

### Single Node

| Cores | Workers | Classifer_t / Worker  | Tempo   | speedup (seq/para)  | Efi. (S/cores)  | Hit     | Unk   |
| ----  | ------  | --------------------  | -----   | ---                 | -----           | ---     | ---   |
| base  | 0       |                       | 1:51.64 | 1                   | 1               | 38.83%  | 5.34% |
| 2     | 1       | 1                     | 4:06.89 | 0.4522              |                 | 35.59%  | 5.43% |
| 3     | 2       | 1                     | 1:44.85 | 1.0648              |                 | 35.47%  | 4.62% |
| 4     | 3       | 1                     | 2:21.42 | 0.7894              |                 | 35.01%  | 7.42% |
| 4     | 4       | 1                     | 2:21.42 | 0.7894              |                 | 35.01%  | 7.42% |

### Single Node, Multi Thread

| Cores | Workers | Classifer_t / Worker  | Tempo     | speedup | Eficiencia  | Hit     | Unk   |
| ----  | ------  | --------------------  | -----     | ---     | -----       | ---     | ---   |
| 4     | 4       | 1                     | 2.99E+02  |  37.36% | 37.36%      | 36.52%  | 6.31% |
| 4     | 4       | 2                     | DNF   |         |             |     |     |
| 4     | 4       | 3                     | -     |         |             |     |     |
| 4     | 4       | 4                     | -     |         |             |     |     |

### Multi Node, Multi Thread

| Nodes | Workers | Tempo     | speedup | Eficiencia  | Hit     | Unk   |
| ----  | ------  | -----     | ---     | -----       | ---     | ---   |
| 1     | 4       | 1.27E+02  | 87.97%  | 29.32%      | 34.69%  | 3.93% |
| 2     | 8       | 1.17E+02  | 95.76%  | 13.68%      | 35.88%  | 5.52% |
| 3     | 12      | 1.27E+02  | 88.22%  | 8.02%       | 35.23%  | 7.42% |

## Reunião 2020-12-08

- [x] ~~RW-lock @ classifier/m_receiver;~~  did't change notth
- [ ] 
