# Quali

A Internet das Coisas (Internet of Things - IoT) é um tema frequentemente
abordado em pesquisas de segurança em redes de computadores claramente motivado
pelo crescimento dessa rede, estimada em 8.4 bilhões de dispositivos em 2017
[1] e, pelo risco que ela representa fundamentado no histórico de ataques
massivos realizados por milhares de nós subvertidos como o realizado pela botnet
mirai em 2016 [2]. Mais preocupante nesse cenário são os fatores que
possibilitaram esses ataques: falta de controle sobre a origem do hardware e
software embarcado nos dispositivos além das cruciais atualizações de segurança.

Com esse desafio de segurança, especialmente em IoT industrial onde a subversão
de dispositivos pode causar danos reais imediatamente, profissionais de
segurança de redes e operadores de redes são confrontados com enorme superfície
de ataque composta por diversos sistemas, tecnologias com longo tempo de vida e
poucas atualizações, um sistema de detecção de intrusão (Intrusion Detection
Systems - IDS) operando na rede de computadores local torna-se uma ferramenta
muito importante para defesa dessa rede e os serviços que ela suporta.

Os IDS foram tradicionalmente construídos à partir de técnicas de mineração de
dados (Data Mining - DM), aprendizado de máquina (Machine Learning - ML), mais
especificamente Detecção de Novidades (Novelty Detection - ND) para detectar
ataques e descobrir novos padrões, porém ao analisar tráfego de rede a
velocidade da análise deve ser próxima ou superior à velocidade da rede
analisada além de não consumir mais recursos do que a própria rede analisada.
Mais restrições nesse sentido devem ser incorporadas quando trata-se de uma rede
IoT, diferente de um rede em Cloud, especialmente latência e banda são ainda
mais restritos e ao mitigar esses atributos movendo o IDS para o mais próximo da
rede IoT passando a processar na névoa computacional (Fog Computing - Fog)
armazenamento e processamento são também restringidos. Portanto uma única
leitura do conjunto analisado, rápida atualização e distribuição do modelo de
detecção e resultados em tempo real são características positivas encontradas
nas técnicas de mineração e processamento de fluxos de dados (Data Streams -
DS).

Nesse contexto, foca-se na arquitetura de IDS proposta por [3] baseada no
algoritmo de ND em DS MINAS [4] e na implementação BigFlow [5] proposta para
redes 10 Gigabit Ethernet. Acredita-se que a fusão dessas abordagens em uma nova
implementação seja capaz de tratar uma rede de maior fluxo com nível comparável
de precisão da análise com o mesmo hardware e maior capacidade de escalonamento
horizontal com distribuição de carga entre nós na Fog.


$ wc -l ../datasets/cassales/KDDTe5Classes_fold1_*
   48791 ../datasets/cassales/KDDTe5Classes_fold1_ini.csv
  442800 ../datasets/cassales/KDDTe5Classes_fold1_onl.csv
  491591 total
$ wc -l ../datasets/kdd/*
  4898431 ../datasets/kdd/kddcup.data
   494021 ../datasets/kdd/kddcup.data_10_percent
  5392452 total
$ wc -l ../datasets/cassales/bases/*
    48791 ../datasets/cassales/bases/KDDTe5Classes_fold1_ini
        0 ../datasets/cassales/bases/aaa
    72000 ../datasets/cassales/bases/kyoto_binario_binarized_offline_1class_fold1_ini
   653457 ../datasets/cassales/bases/kyoto_binario_binarized_offline_1class_fold1_onl
    72000 ../datasets/cassales/bases/kyoto_binario_offline_1class_fold1_ini
   653457 ../datasets/cassales/bases/kyoto_binario_offline_1class_fold1_onl
    72000 ../datasets/cassales/bases/kyoto_multiclasse_binarized_offline_1class_fold1_ini
   653457 ../datasets/cassales/bases/kyoto_multiclasse_binarized_offline_1class_fold1_onl
    72000 ../datasets/cassales/bases/kyoto_multiclasse_binarized_offline_2class_fold1_ini
   653457 ../datasets/cassales/bases/kyoto_multiclasse_binarized_offline_2class_fold1_onl
    72000 ../datasets/cassales/bases/kyoto_multiclasse_offline_1class_fold1_ini
   653457 ../datasets/cassales/bases/kyoto_multiclasse_offline_1class_fold1_onl
    72000 ../datasets/cassales/bases/kyoto_multiclasse_offline_2class_fold1_ini
   653457 ../datasets/cassales/bases/kyoto_multiclasse_offline_2class_fold1_onl
        0 ../datasets/cassales/bases/out_
  4401533 total