#!/usr/bin/python3

import sys
from pandas import DataFrame, read_csv
import matplotlib.pyplot as plt
import pandas as pd 

examplesDf = pd.read_csv(filepath_or_buffer=sys.argv[1], header=None)
examplesDf['class'] = examplesDf[22]
examplesDf['#pointId'] = examplesDf.index

matchesDf = pd.read_csv(filepath_or_buffer=sys.argv[2])

df = pd.merge(matchesDf, examplesDf, on='#pointId')
print("Confusion Matrix")
confusion_matrix = pd.crosstab(df['label'], df['class'], colnames=['Classes (act)'], rownames=['Labels (pred)'])
print(confusion_matrix)

# Total examples        653457
# Total examples     653457
# Total matches      653457
# Hits               205961 ( 31.518677%)
# Misses             447496 ( 68.481323%)
totalExamples = examplesDf['#pointId'].count()
totalMatches = matchesDf['#pointId'].count()
hits = df[df['label'] == df["class"]]['#pointId'].count()
misses = df[df['label'] != df["class"]]['#pointId'].count()

print('Total examples   %8d' % (totalExamples))
print('Total matches    %8d' % (totalMatches))
print('Hits             %8d (%10f%%)' % (hits, (hits/totalExamples) * 100.0))
print('Misses           %8d (%10f%%)' % (misses, (misses/totalExamples) * 100.0))
print('Hits + Misses    %8d (%10f%%)' % (hits + misses, ((hits + misses)/totalExamples) * 100.0))
