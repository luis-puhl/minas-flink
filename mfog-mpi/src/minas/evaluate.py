#!/usr/bin/python3

import sys
from pandas import DataFrame, read_csv
import matplotlib.pyplot as plt
import pandas as pd
import os.path

examplesFilename = sys.argv[1]
matchesFilename = sys.argv[2]
err = 0
if not os.path.isfile(examplesFilename):
    print('file not found   %s' % (examplesFilename))
    err += 1
if not os.path.isfile(matchesFilename):
    print('file not found   %s' % (matchesFilename))
    err += 1
if err > 0:
    exit(1)

examplesDf = pd.read_csv(filepath_or_buffer=examplesFilename, header=None)
examplesDf['class'] = examplesDf[22]
examplesDf['#pointId'] = examplesDf.index

matchesDf = pd.read_csv(filepath_or_buffer=matchesFilename)

df = pd.merge(matchesDf, examplesDf, on='#pointId')
confusion_matrix = pd.crosstab(df['class'], df['label'], rownames=['Classes (act)'], colnames=['Labels (pred)'])
confusion_matrix['total'] = sum([confusion_matrix[col] for col in confusion_matrix.columns])
confusion_matrix = confusion_matrix.transpose()
confusion_matrix['total'] = sum([confusion_matrix[col] for col in confusion_matrix.columns])
print("Confusion Matrix")
# confusion_matrix = pd.crosstab(df['label'], df['class'], colnames=['Classes (act)'], rownames=['Labels (pred)'])
print(confusion_matrix)

# Total examples     653457
# Total examples     653457
# Total matches      653457
# Hits               205961 ( 31.518677%)
# Misses             447496 ( 68.481323%)
totalExamples = examplesDf['#pointId'].count()
totalMatches = matchesDf['#pointId'].count()
hits = df[df['label'] == df["class"]]['#pointId'].count()
misses = df[df['label'] != df["class"]]['#pointId'].count()

print('\n')
print('Total examples   %8d' % (totalExamples))
print('Total matches    %8d' % (totalMatches))
print('Hits             %8d (%10f%%)' % (hits, (hits/totalExamples) * 100.0))
print('Misses           %8d (%10f%%)' % (misses, (misses/totalExamples) * 100.0))
print('Hits + Misses    %8d (%10f%%)' % (hits + misses, ((hits + misses)/totalExamples) * 100.0))
