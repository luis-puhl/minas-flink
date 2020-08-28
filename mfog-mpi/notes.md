
- N0-T0: dispach
    - 2: classify
    - 3: classify
    - ...
    - N: classify
- N0-T1: recv match -> ND


$ sudo vi /etc/resolv.conf 

...

nameserver 200.18.99.1
# nameserver 127.0.0.53
...
$ sudo ip route add default via 192.168.0.1

# Resumo semana 30 de 2020

## Modelos continuam não idênticos (porém bem mais próximos)

Na semana passada discutimos a implementação em `c` da construção do modelo inicial
com o algoritmo k-means.

Para a implementação utilizei um script `python` comparando o modelo original
com o criado pela implementação através de gráficos. Nessa comparação foi fácil
visualizar as diferenças e fazer alguns ajustes.

Na reunião encontrei uma divergência entre a implementação original e a nova, no
método de classificação de um exemplo, na comparação entre raio e distância
mínima, que resultou em um modelo mais próximo.

(git commit 2947ddfc83dc6b711f810200108cb3be9b4f3560)
Após a reunião outra alteração, na construção do modelo após o agrupamento,
para o cálculo de raio, os artigos sugerem usar o desvio padrão das distâncias
dos pontos que compõem o cluster, porém na implementação a distância máxima é
utilizada. Essa alteração minimizou a diferença entre o modelo criado e o original.

(git commit 7c890fa59fe268741ce53946690bab751b83bf00)
Feito isto, voltei ao código da implementação original e revisei a criação do
modelo inicial, tentando simplificar a implementação e manter o algoritmo.

Porém não tive sucesso, o resultado mudou em comparação com o modelo estático,
então assumi que a diferença entre a implementação original e a nova era aceitável.


### Confusion Matrix

| Minas  | |        |          |          | Mfog  |  |          |          |         |
|---| ---| ---| ---| ---| ---| ---| ---| ---| ---|
| Classes (act)  |      A |       N | assigned |    hits  | Classes (act)  |       A |        N | assigned |   hits  |
| Labels (pred)  |        |         |          |          | Labels (pred)  |         |          |          |         |
| -              |   3774 |    8206 |        - |       0  | -              |    8306 |      355 |        - |       0 |
| 1              |    123 |       0 |        A |     123  | N              |  435763 |   205887 |        N |  205887 |
| 10             |   3520 |    5130 |        N |    5130  | a              |     236 |        0 |        A |     236 |
| 11             |     71 |     289 |        N |     289  | b              |     756 |       81 |        A |     756 |
| 12             |     26 |       0 |        A |      26  | c              |     574 |        0 |        A |     574 |
| 2              |    152 |      82 |        A |     152  | d              |     214 |        0 |        A |     214 |
| 3              |    368 |      44 |        A |     368  | e              |     243 |        0 |        A |     243 |
| 4              |      8 |       0 |        A |       8  | f              |     499 |        0 |        A |     499 |
| 5              |     82 |       1 |        A |      82  | g              |     431 |        0 |        A |     431 |
| 6              |    165 |       0 |        A |     165  | h              |     495 |        0 |        A |     495 |
| 7              |      8 |     396 |        N |     396  | i              |     237 |        0 |        A |     237 |
| 8              |   1054 |     183 |        A |    1054  | j              |     640 |        0 |        A |     640 |
| 9              |    161 |     154 |        A |     161  | k              |      57 |        0 |        A |      57 |
| N              | 441395 |  199715 |        N |  199715  | l              |     539 |        0 |        A |     539 |
|                |        |         |          |          | m              |      55 |        0 |        A |      55 |
|                |        |         |          |          | n              |      91 |        0 |        A |      91 |
|                |        |         |          |          | o              |      69 |        0 |        A |      69 |
|                |        |         |          |          | p              |    3464 |        0 |        A |    3464 |
|                |        |         |          |          | q              |     169 |        0 |        A |     169 |
|                |        |         |          |          | r              |     269 |        0 |        A |     269 |

### Confusion Matrix Summary

| Summary           | Minas                 | Mfog      |
|---                | ---:                   | ---:|
| Total examples    | (653457, 25)          | (653457, 25)                      |
| Total matches     | (665107, 9)           | (659430, 9)                       |
| Hits              |  207669 ( 31.223397%) |  214925 ( 32.592542%)             |
| Misses            |  445458 ( 66.975389%) |  435844 ( 66.094051%)             |
| Unknowns          |   11980 (  1.801214%) |    8661 (  1.313407%)             |
| Unk. reprocessed  |   11650 ( 97.245409%) |    5973 ( 68.964323%)             |

## Implementado ND com alguns resultados favoráveis

Com o modelo inicial estabelecido adição da detecção de novidades foi simples.

Acompanhando a implementação de ND, o script de avaliação foi modificado para
tratar de múltiplas classes e múltiplos rótulos.

Além disso, um gráfico de acertos, erros e desconhecidos foi adicionado à avaliação.

Função noveltyDetection, 4 execuções com total de 1.24s:

- 3.585160e-01,n=2000
- 4.169170e-01,n=2000
- 2.498920e-01,n=2000
- 2.157580e-01,n=2000

## Próximos passos

- [ ] Escrever um *teste de mesa* para k-means com `csv` de entrada,
    pequeno (K, d, n) (número de clusters, dimensões e exemplos),
    plot dos pontos e centroides; (K<=10, d=2, n<=100)
- [ ] Revisão das métricas de erro (CER, Unk, FMacro);
- [ ] CluStream vs k-means (k-means é muito lento para o modelo inicial, mas aceitável para ND);
- [ ] Envio de modelo e exemplos por socket/redis;
- [ ] Tempo entre criação do exemplo e classificação;

## Meeting 2020-08-05

Seguir o paper.
Justificar escolhas que divergem.
Implementação atual é razoável.
Mudanças nas decisões são resultados.

Estudo da distribuição do dataset em cada um dos nós da borda.

Comparação serial paralelo apenas.

## Meeting 2020-08-26 (and others in August)

- Desafio, resposta, justificativa.
- Artigo para setembro ou outubro.
- Revisão dos valores da avaliação.

### Desafios, Respostas e Justificativas

Desafios de arquitetura e validação:

- Construção de um protótipo da arquitetura IDSA-IoT:
  - Kafka (Python): Distribuição e balanceamento pelo cluster kafka, hipótese refutada.
  - Flink (Java ou Scala): Execução do cluster nos dispositivos de névoa, hipótese refutada.
  - MPI (C e Python): Execução do cluster nos dispositivos de névoa, hipótese aceita.
- Reimplementação do algoritmo MINAS com fidelidade:
  - Duas versões: a descrita e a implementação de referência (em Java).
  - Resolução: utilizar a descrição, não *seguir* a imp. referência, apenas como ponto de comparação. Exemplos:
    - Definição de raio `r = f * σ` (fator vezes desvio padrão) para `r = max(distance)` (distância máxima);
    - Tamanho do buffer de desconhecidos e frequência de execução do passo de detecção de novidade;

Desafios de implementação:

<!-- - Definição de raio: desvio padrão das distâncias versus distancia máxima;
- Atualização do micro-cluster limita-se à atualização do atributo \texttt{T};
- Remoção de exemplos na implementação de referência é feita somente para o algoritmo \textit{CluStream};
- Inclusão de borda: algoritmo inclui ($<=$), referência não inclui ($<$);
- Seguiu-se as mesmas divergências anteriores para comparação dos resultados com a implementação referência;
- Inclusão da borda;
- Comportamento do mecânismo de \textit{sleep-model} não está definido, portanto não está ativo;
- Processo de clusterização é limitado ao algoritmo \textit{K-Means}. Algoritmo \textit{CluStream} não está implementado; -->

- `Double vs Float`:
  - Na implementação de referência, java double é utilizado;
  - Na nova implementação duas versões foram testadas e a diferença de precisão entre as duas é de `5 E-8`;
  - **Solução:** Use `float32` e economize os bits já que haverá comunicação entre nós e módulos;
- Formato do fluxo de saída:
  - Implementação de referência utiliza a tripla `(id, classe, etiqueta)`;
  - Primeira implementação em C utiliza `(id, clusterLabel, clusterId, clusterRadius, label, distance, secondDistance)`;
  - Segunda implementação utiliza dupla `(id, label)`;
  - Na etapa de avaliação, independente de versão, o fluxo original é lido;
  - **Solução:** O formato mínimo é `(id, label)`;
- Reprocessamento dos exemplos utilizados para atualização do modelo:
  - Muda o comportamento do operador de fluxo de `Map` para `Flatmap`, ou seja,
    requer outro fluxo de saída para a transmissão de padrões novidade (alarmes);
  - Para reclassificação a definição de raio é modificada de `r = f * σ` (fator
    multiplicando desvio padrão) para `r = max(distance)` (distância máxima);
  - Passível da crítica de *overfitting*. Isto é, este processo pode
    inflar a métrica de precisão;
  - **Solução:** *em aberto*;

Próximos desafios:

- Distribuição e paralelização para minimização de latência entre novo item no fluxo e sua classificação:
  - Tempo de passagem da instancia pelo classificador;
  - Volume máximo do sistema;
  - Diferenças de precisão de acordo com a carga;
- Detecção de novidades e manutenção de modelo em ambiente distribuído:
  - Mecanismo de ND local (síncrono) vs nuvem quanto à atraso de definição de modelo (nesse ponto é onde a hipótese prevê maior diferença, grande ponto de interesse);
  - Mecanismo de esquecimento local vs global (modelo único ou por nó);
  - Atraso na reclassificação dos desconhecidos;
