 #!/bin/bash          
    echo Hello World
    #multiclasse binarizado 1 classe
    python3 script.py sem_prep -t 10000 -c 14 17 -r 13 15 16 18 19 20 21 22 -b 1 23 -n 0 2 3 4 8 9 -s -p 10 -k 0,1 --suffix 1class
    # mv sem_prep_binarized_all kyoto_multiclasse_binarized_full
    mv sem_prep_binarized_offline_1class kyoto_multiclasse_binarized_offline_1class
    mv sem_prep_binarized_online_1class kyoto_multiclasse_binarized_online_1class
    cat kyoto_multiclasse_binarized_offline_1class > kyoto_multiclasse_binarized_1class_full
    cat kyoto_multiclasse_binarized_online_1class >> kyoto_multiclasse_binarized_1class_full

    #multiclasse binarizado 2 classes
    python3 script.py sem_prep -t 10000 -c 14 17 -r 13 15 16 18 19 20 21 22 -b 1 23 -n 0 2 3 4 8 9 -s -p 10 -k 0,1 19187-3-7,-1 --suffix 2class
    mv sem_prep_binarized_offline_2class kyoto_multiclasse_binarized_offline_2class
    mv sem_prep_binarized_online_2class kyoto_multiclasse_binarized_online_2class
    cat kyoto_multiclasse_binarized_offline_2class > kyoto_multiclasse_binarized_2class_full
    cat kyoto_multiclasse_binarized_online_2class >> kyoto_multiclasse_binarized_2class_full

    #multiclasse sem binarizar (removido 1 e 23) 1 classe
    python3 script.py sem_prep -t 10000 -c 14 17 -r 13 15 16 18 19 20 21 22 1 23 -n 0 2 3 4 8 9 -s -p 10 -k 0,1 --suffix 1class
    # mv sem_prep_all kyoto_multiclasse_full
    mv sem_prep_offline_1class kyoto_multiclasse_offline_1class
    mv sem_prep_online_1class kyoto_multiclasse_online_1class
    cat kyoto_multiclasse_offline_1class > kyoto_multiclasse_1class_full
    cat kyoto_multiclasse_online_1class >> kyoto_multiclasse_1class_full

    #multiclasse sem binarizar (removido 1 e 23) 2 classes
    python3 script.py sem_prep -t 10000 -c 14 17 -r 1 13 15 16 18 19 20 21 22 23 -n 0 2 3 4 8 9 -s -p 10 -k 0,1 19187-3-7,-1 --suffix 2class
    mv sem_prep_offline_2class kyoto_multiclasse_offline_2class
    mv sem_prep_online_2class kyoto_multiclasse_online_2class
    cat kyoto_multiclasse_offline_2class > kyoto_multiclasse_2class_full
    cat kyoto_multiclasse_online_2class >> kyoto_multiclasse_2class_full

#####
    #binario binarizado 1 classe
    python3 script.py sem_prep -t 10000 -c 17 -r 13 14 15 16 18 19 20 21 22 -b 1 23 -n 0 2 3 4 8 9 -s -p 10 -k 0,1 --suffix 1class
    # mv sem_prep_binarized_all kyoto_binario_binarized_full
    mv sem_prep_binarized_offline_1class kyoto_binario_binarized_offline_1class
    mv sem_prep_binarized_online_1class kyoto_binario_binarized_online_1class
    cat kyoto_binario_binarized_offline_1class > kyoto_binario_binarized_full
    cat kyoto_binario_binarized_online_1class >> kyoto_binario_binarized_full

    #binario binarizado 2 classes -> NEcessário?
    #python3 script.py sem_prep -t 10000 -c 17 -r 13 14 15 16 18 19 20 21 22 -b 1 23 -n 0 2 3 4 8 9 -s -p 10 -k 0,1 19187-3-7,-1 0,1 21355-3-4,-1 0,1 384-1-8,-1 0,1 1917-1-15,-1 0,1 6-128-2,-1 0,1 402-1-15,-1 0,1 28556-1-2,-1 0,1 2049-1-8,-1 --suffix 2class
    #mv sem_prep_binarized_offline_2class kyoto_binario_binarized_offline_2class
    #mv sem_prep_binarized_online_2class kyoto_binario_binarized_online_2class

    #binario sem binarizar (removido 1 e 23) 1 classe
    python3 script.py sem_prep -t 10000 -c 17 -r 13 14 15 16 18 19 20 21 22 1 23 -n 0 2 3 4 8 9 -s -p 10 -k 0,1 --suffix 1class
    # mv sem_prep_all kyoto_binario_full
    mv sem_prep_offline_1class kyoto_binario_offline_1class
    mv sem_prep_online_1class kyoto_binario_online_1class
    cat kyoto_binario_offline_1class > kyoto_binario_full
    cat kyoto_binario_online_1class >> kyoto_binario_full

    #binario sem binarizar (removido 1 e 23) 2 classes  -> NEcessário?
    #python3 script.py sem_prep -t 10000 -c 17 -r 1 13 14 15 16 18 19 20 21 22 23 -n 0 2 3 4 8 9 -s -p 10 -k 0,1 19187-3-7,-1 0,1 21355-3-4,-1 0,1 384-1-8,-1 0,1 1917-1-15,-1 0,1 6-128-2,-1 0,1 402-1-15,-1 0,1 28556-1-2,-1 0,1 2049-1-8,-1 --suffix 2class
    #mv sem_prep_offline_2class kyoto_binario_offline_2class
    #mv sem_prep_online_2class kyoto_binario_online_2class

