#!/usr/bin/python3
# coding: utf-8

import sys
import os.path
import numpy as np
import pandas as pd
import matplotlib
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm
# get_ipython().run_line_magic('matplotlib', 'inline')
# plt.close('all')

from plotHitMissUnkRate import plotHitMissUnkRate

def getExamplesDf(path):
    assert os.path.isfile(path), "file '%s' not found." % path
    df = pd.read_csv(filepath_or_buffer=path, header=None)
    df['id'] = df.index
    df['class'] = df[22]
    return df

def cleanLabel(label):
    if label == 'Unk':
        return '-'
    for t in ['C ', 'N ', 'ExtCon ', 'ExtNov ']:
        if label.startswith(t):
            label = label.replace(t, '').strip()
    return label

def getMatchesDf(path):
    assert os.path.isfile(path), "file '%s' not found." % path
    with open(path) as matchesFile:
        firstLine = matchesFile.read(10)
        # print('firstLine', firstLine)
        isOriginalFormatWithHeader = firstLine.startswith('Results')
        isOriginalFormat = firstLine.startswith('Ex: ') or isOriginalFormatWithHeader
        isMfog = firstLine.startswith('#pointId,')
    # 
    if isMfog:
        try:
            df = pd.read_csv(filepath_or_buffer=path)
            df['id'] = df['#pointId'].astype(int)
            df['og_label'] = df['label']
            return df
        except pd.errors.ParserError as err:
            print((err, path))
            raise(err)
    # 
    if isOriginalFormat:
        if isOriginalFormatWithHeader:
            df = pd.read_table(filepath_or_buffer=path, header=None, skiprows=4)
        else:
            df = pd.read_table(filepath_or_buffer=path, header=None)
        df.columns = ['id', 'class', 'label']
        df = df[df['id'].str.startswith('Ex:')]

        df['label'] = df['label'].apply(lambda x: x.replace('Classe MINAS:', '').strip())
        return pd.DataFrame({
            'id': df['id'].apply(lambda x: x.replace('Ex:', '').strip()).astype(int) - 1,
            'label': df['label'].apply(cleanLabel),
            'og_label': df['label'],
        })
    return False

def printEval(exDf, maDf, plotSavePath=None, title=None):
    # df = merge(exDf, maDf)
    # cf, classes, labels, off, ass = confusionMatrix(df)
    assert pd.Series(['id', 'class']).isin(exDf.columns).all()
    assert pd.Series(['id', 'label']).isin(maDf.columns).all()
    #
    merged = pd.merge(exDf[['id', 'class']], maDf[['id', 'label']], on='id', how='left')
    merged['label'] = merged['label'].fillna('N')
    # assert merged.columns.all(['id', 'class', 'label'])
    cf = pd.crosstab(merged['class'], merged['label'],
                     rownames=['Classes (act)'], colnames=['Labels (pred)']).transpose()
    classes = cf.columns.values
    labels = cf.index.to_list()
    off = ['-'] + [c for c in classes if c in labels]
    cf['assigned'] = [l if l in off else c for l, c in zip(labels, cf.idxmax(axis='columns'))]
    cf['hits'] = [0 if l == '-' else cf.at[i, l] for i, l in cf['assigned'].iteritems()]
    assignment = dict([ v for v in cf['assigned'].iteritems()])
    ogLabels = maDf['og_label'].unique()
    ogLabelsMap = { k: [] for k in labels }
    if len(labels) != len(ogLabels):
        for og in ogLabels:
            ogLabelsMap[cleanLabel(og)] += [og]
        cf['og_labels'] = [ogLabelsMap[l] for l in cf.index ]
    #
    # return (cf, classes, labels, off, assignment)
    # 
    df, labelSet = plotHitMissUnkRate(merged, assignment, off, plotSavePath, title)
    print("Confusion Matrix")
    with pd.option_context('display.max_rows', None, 'display.max_columns', None):  # more options can be specified also
        print(cf)

    totalExamples = exDf.shape[0]
    totalMatches = maDf.shape[0]
    tot = max(totalMatches, totalExamples)
    hits = df['hit'].sum()
    misses = df['miss'].sum()
    unks = df['unk'].sum()
    repro = totalMatches - totalExamples

    print('Classes          ', classes)
    print('Initial labels   ', off)
    print('Labels (item)    ', labelSet, len(labelSet))
    # print('Assignment       ', assignment)
    print('Total examples   ', exDf.shape)
    print('Total matches    ', maDf.shape)
    print('Hits             %8d (%10f%%)' % (hits, (hits/tot) * 100.0))
    print('Misses           %8d (%10f%%)' % (misses, (misses/tot) * 100.0))
    print('Unknowns         %8d (%10f%%)' % (unks, (unks/tot) * 100.0))
    print('Unk. reprocessed %8d (%10f%%)' % (repro, (repro/unks) * 100.0))
    print('Total            %8d (%10f%%)' % (hits + misses + unks, ((hits + misses + unks)/tot) * 100.0))
    return df, cf, classes, labels, off, assignment, ogLabelsMap

def main(
    title='Reference',
    examplesFileName='datasets/test.csv',
    matchesFileName='out/og/kmeans-nd/results',
    plotSavePath='out/hits.png',
):
    if len(plotSavePath) == 0:
        plotSavePath = None
    print(
        'Evaluate\n' +
        '\ttitle = %s\n' % title +
        '\texamplesFileName = %s\n' % examplesFileName +
        '\tmatchesFileName = %s\n' % matchesFileName +
        '\tplotSavePath = %s\n' % plotSavePath
    )
    examplesDf = getExamplesDf(examplesFileName)
    countPerClass = examplesDf.groupby('class').count()[['id']]
    print("Count per class")
    print(countPerClass)

    matchesDf = getMatchesDf(matchesFileName)
    
    print("### "+title+"\n")
    return examplesDf, matchesDf, printEval(examplesDf, matchesDf, plotSavePath, title)

if __name__ == "__main__":
    params = [
        'title',
        'examplesFileName',
        'matchesFileName',
        'plotSavePath',
    ]
    r = main(**dict(zip(params, sys.argv[1:])))
    print('')
