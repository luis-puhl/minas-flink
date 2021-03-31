#!/usr/bin/python3
# coding: utf-8

import sys
import os.path
import re
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
            df['og_label'] = df['label'].str.strip()
            df['lag'] = df['lag'].astype(int)
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

def confusionMatrix(exDf, maDf):
    assert pd.Series(['id', 'class']).isin(exDf.columns).all()
    assert pd.Series(['id', 'label']).isin(maDf.columns).all()
    #
    merged = pd.merge(exDf[['id', 'class']], maDf[['id', 'label']], on='id', how='left')
    nanLabels = merged['label'].isna().sum()
    merged['label'] = merged['label'].fillna('N')
    # assert merged.columns.all(['id', 'class', 'label'])
    cf = pd.crosstab(merged['class'], merged['label'],
                     rownames=['Classes'], colnames=['Labels']).transpose()
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
    return (cf, merged, classes, labels, off, assignment, ogLabelsMap, nanLabels)

def fixTex(tex):
    rep = {'\\midrule': '\\hline', '\\toprule': '', '\\\\\n\\bottomrule': '', '\\\\': '\\\\\hline', 'Assigned': '\hline\nAssigned'}
    for a in rep:
        tex = tex.replace(a, rep[a])
    r = re.compile(r'\{l([lr]+)\}')
    ma = r.search(tex)
    if ma:
        tex = tex.replace(ma.group(0), '{l' + ma.group(1).replace('r', 'l').replace('l', '|r') + '}')
    return tex

def getConfusionMatrixTex(exDf, maDf, off):
    merged = pd.merge(exDf[['id', 'class']], maDf[['id', 'label']], on='id', how='left')
    cf = pd.crosstab(merged['class'], merged['label'], rownames=['Classes'], colnames=['Labels'])
    cf = cf[sorted(list(cf.columns), key=lambda x: int(x) if x.isnumeric() else -1)]
    cf = cf.transpose()
    cf['Assigned'] = [l if l in off else c for l, c in zip(cf.index.to_list(), cf.idxmax(axis='columns'))]
    cf['Hits'] = [0 if l == '-' else cf.at[i, l] for i, l in cf['Assigned'].iteritems()]
    cf = cf.transpose()
    # 
    return fixTex(cf.to_latex())

def getTimeFromLog(logPath):
    user = -1
    system = -1
    elapsed = -1
    try:
        with open(logPath, 'r') as f:
            content = f.readlines()
            for line in content:
                if 'user	' in line:
                    r = re.compile(r'(\d+\.\d+) user	(\d+\.\d+) system	(\d+):(\d+\.\d+) elapsed')
                    m = r.search(line)
                    user = float(m.group(1))
                    system = float(m.group(2))
                    elapsed = float(m.group(3)) * 60 + float(m.group(4))
    except IOError:
        print("Timing not available")
    return { 'user': user, 'system': system, 'elapsed': elapsed }

def printEval(exDf, maDf, logPath=None, title=None):
    # df = merge(exDf, maDf)
    cf, merged, classes, labels, off, assignment, ogLabelsMap, nanLabels = confusionMatrix(exDf, maDf)
    # 
    df, labelSet, xcoords, allPos = plotHitMissUnkRate(merged, assignment, off, logPath + '.png', title)
    print('NaN labels:', nanLabels)
    print("Confusion Matrix")
    with pd.option_context('display.max_rows', None, 'display.max_columns', None):  # more options can be specified also
        print(cf)

    totalExamples = exDf.shape[0]
    totalMatches = maDf.shape[0]
    tot = max(totalMatches, totalExamples)
    hits = df['hit'].sum()
    misses = df['miss'].sum()
    unknowns = df['unk'].sum()
    repro = totalMatches - totalExamples
    # 
    lag = 0
    if 'lag' in maDf:
        lag = maDf['lag'].mean()
        # 
        maDf['lag'] = maDf['lag'] * 10e-8
        y_mean = [lag * 10e-8] * max(maDf.index)
        dpi=300
        ax = maDf[['lag']].plot(title=title + ' lag', figsize=(1920 / dpi, 1080 / dpi), legend=False, markersize=5)
        mean_line = ax.plot(y_mean, label='mean', linestyle='--')
        legend = ax.legend(loc='upper right', fontsize='xx-small')
        maxTime = max(maDf['lag'])
        minTime = min(maDf['lag'])
        tenth = (maxTime - minTime) * 0.04
        ax.vlines(x=xcoords, ymin=(minTime - tenth), ymax=(maxTime + tenth), colors='gray', ls='--', lw=0.5, label='vline_multiple')
        ax.get_xaxis().set_major_formatter(matplotlib.ticker.EngFormatter())
        ax.get_yaxis().set_major_formatter(matplotlib.ticker.EngFormatter(unit='s'))
        plotSavePath = logPath.replace('.log', '-lag.log.png')
        plt.savefig(plotSavePath, dpi=dpi, bbox_inches='tight')

    print('Classes          ', classes)
    print('Initial labels   ', off)
    print('Labels (item)    ', labelSet, len(labelSet))
    # print('Assignment       ', assignment)
    print('Total examples   ', exDf.shape)
    print('Total matches    ', maDf.shape)
    print('Hits             %8d (%10f%%)' % (hits, (hits/tot) * 100.0))
    print('Misses           %8d (%10f%%)' % (misses, (misses/tot) * 100.0))
    print('Unknowns         %8d (%10f%%)' % (unknowns, (unknowns/tot) * 100.0))
    print('Unk. reprocessed %8d (%10f%%)' % (repro, (repro/unknowns) * 100.0))
    print('Total            %8d (%10f%%)' % (hits + misses + unknowns, ((hits + misses + unknowns)/tot) * 100.0))
    print('Avg Time Thru    %8d (%10fs)' % (lag, lag * 10e-8))
    #
    tm = getTimeFromLog(logPath)
    resume = pd.DataFrame({
        'Metric': ['Hits', 'Misses', 'Unknowns', 'Time', 'System', 'Elapsed' ],
        'Value': [hits/tot, misses/tot, unknowns/tot, tm['user'], tm['system'], tm['elapsed'] ],
    }).set_index('Metric')
    resumeTex = fixTex(resume.to_latex()).replace('{}', 'Metric  ').replace('Metric   &             \\\\\hline\n', '')
    print(resumeTex)
    print(resume)
    with open(logPath + '.tex', 'w') as f:
        f.write(getConfusionMatrixTex(exDf, maDf, off))
        f.write(resumeTex)
    #
    return df, cf, classes, labels, off, assignment, ogLabelsMap

def main(
    title='Reference',
    examplesFileName='datasets/test.csv',
    matchesFileName='out/og/kmeans-nd/results',
    logPath='experiments/online-nd.log',
):
    if len(logPath) == 0:
        logPath='experiments/online-nd.log'
    print(
        'Evaluate\n' +
        '\ttitle = %s\n' % title +
        '\texamplesFileName = %s\n' % examplesFileName +
        '\tmatchesFileName = %s\n' % matchesFileName +
        '\tlogPath = %s\n' % logPath
    )
    examplesDf = getExamplesDf(examplesFileName)
    countPerClass = examplesDf.groupby('class').count()[['id']]
    print("Count per class")
    print(countPerClass)

    matchesDf = getMatchesDf(matchesFileName)
    
    print("### "+title+"\n")
    return examplesDf, matchesDf, printEval(examplesDf, matchesDf, logPath, title)

if __name__ == "__main__":
    params = [
        'title',
        'examplesFileName',
        'matchesFileName',
        'logPath',
    ]
    r = main(**dict(zip(params, sys.argv[1:])))
    print('')
