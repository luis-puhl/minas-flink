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

dpi = 300
figsize = (1920 / dpi, 1080 / dpi)

def getExamplesDf(path):
    assert os.path.isfile(path), "file '%s' not found." % path
    df = pd.read_csv(filepath_or_buffer=path, header=None)
    df['id'] = df.index
    df['class'] = df[22]
    return df

def getOriginalMatchesDf(path):
    assert os.path.isfile(path), "file '%s' not found." % path
    df = pd.read_table(filepath_or_buffer=path, header=None)
    df.columns = ['id', 'class', 'label']
    df = df[df['id'].str.startswith('Ex:')]

    def cleanLabel(text):
        label = text.replace('Classe MINAS:', '').strip()
        if label == 'Unk':
            return '-'
        if label.startswith('C '):
            return label.replace('C ', '')
        return label
    return pd.DataFrame({
        'id': df['id'].apply(lambda x: x.replace('Ex:', '').strip()).astype(int) - 1,
        'label': df['label'].apply(cleanLabel),
    })

def getMatchesDf(path):
    assert os.path.isfile(path), "file '%s' not found." % path
    df = pd.read_csv(filepath_or_buffer=path)
    df['id'] = df['#pointId'].astype(int)
    return df

def merge(exDf, maDf):
    def checkCols(df, cols):
        return pd.Series(cols).isin(df.columns).all()
    assert checkCols(exDf, ['id', 'class'])
    assert checkCols(maDf, ['id', 'label'])
    return pd.merge(exDf[['id', 'class']], maDf[['id', 'label']], on='id', how='left')

def confusionMatrix(exDf, maDf=None):
    merged = exDf
    if maDf is not None:
        merged = merge(exDf, maDf)
    assert merged.columns.all(['id', 'class', 'label'])
    cf = pd.crosstab(merged['class'], merged['label'],
                     rownames=['Classes (act)'], colnames=['Labels (pred)']).transpose()
    classes = cf.columns.values
    labels = cf.index.to_list()
    offf = ['-'] + [c for c in classes if c in labels]
    cf['assigned'] = [l if l in offf else c for l, c in zip(labels, cf.idxmax(axis='columns'))]
    cf['hits'] = [0 if l == '-' else cf.at[i, l] for i, l in cf['assigned'].iteritems()]
    cf['misses'] = [0 if l == '-' else cf.at[i, l] for i, l in cf['assigned'].iteritems()]
    # cf['lbl_tot'] = [c for c in classes if c in labels]
    assignment = dict([ v for v in cf['assigned'].iteritems()])
    return (cf, classes, labels, offf, assignment)

def printEval(exDf, maDf, path=None, title=None):
    df = merge(exDf, maDf)
    cf, classes, labels, off, ass = confusionMatrix(df)
    df = plotHitMissUnkRate(df, ass, off, path, title)
    print("Confusion Matrix")
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
    print('Labels           ', labels, len(labels))
    # print('Assignment       ', ass)
    print('Total examples   ', exDf.shape)
    print('Total matches    ', maDf.shape)
    print('Hits             %8d (%10f%%)' % (hits, (hits/tot) * 100.0))
    print('Misses           %8d (%10f%%)' % (misses, (misses/tot) * 100.0))
    print('Unknowns         %8d (%10f%%)' % (unks, (unks/tot) * 100.0))
    print('Unk. reprocessed %8d (%10f%%)' % (repro, (repro/unks) * 100.0))
    print('Total            %8d (%10f%%)' % (hits + misses + unks, ((hits + misses + unks)/tot) * 100.0))
    print('')
    return df, cf, classes, labels, off, ass

def plotHitMissUnkRate(df, assignment, off, path=None, title=None):
    if (path is not None):
        path = path + 'hits_' + title + '.png'
    df['assigned'] = df['label'].map(assignment)
    df['hit'] = (df['assigned'] == df['class']).map({False: 0, True: 1})
    df['miss'] = (df['assigned'] != df['class']).map({False: 0, True: 1})
    df['unk'] = (df['assigned'] == '-').map({False: 0, True: 1})
    df['miss'] = df['miss'] - df['unk']
    # 
    df['hits'] = df['hit'].cumsum()
    df['misses'] = df['miss'].cumsum()
    df['unks'] = df['unk'].cumsum()
    # 
    df['tot'] = df['hits'] + df['misses'] + df['unks']
    # 
    df['d_hit'] = df['hits'] / df['tot']
    df['d_mis'] = df['misses'] / df['tot']
    df['d_unk'] = df['unks'] / df['tot']
    # 
    labelSet = set()
    xcoords = []
    prevLen = len(off)
    for i, l in zip(df.index, df['label']):
        labelSet.add(l)
        if len(labelSet) > prevLen:
            prevLen = len(labelSet)
            xcoords += [i]
    # 
    if (title is not None):
        title += ' Hit Miss Unk'
    else:
        title = 'Hit Miss Unk'
    ax = df[['d_hit', 'd_mis', 'd_unk' ]].plot(title=title, figsize=figsize)
    ax.vlines(x=xcoords, ymin=-0.05, ymax=1.05, colors='gray', ls='--', lw=0.5, label='vline_multiple')
    ax.get_xaxis().set_major_formatter(matplotlib.ticker.EngFormatter())
    # 
    if (path is not None):
        plt.savefig(path, dpi=dpi, bbox_inches='tight')
        print('saving', path)
    return df

def diffMinasMfog(examplesDf, minasDF, mfogDF, path=None):
    print("### Minas\n")
    printEval(examplesDf, minasDF, path, 'minas')
    print("### Mfog\n")
    printEval(examplesDf, mfogDF, path, 'mfog')
    # [['id', 'og', 'label']]
    m = pd.merge(minasDF, mfogDF, on='id', how='left')
    diff = m[m['label_x'] != m['label_y']]
    toRename = {'clusterLabel_x': 'j_clL', 'clusterRadius_x': 'j_clR',
                'label_x': 'j_L', 'distance_x': 'j_D',
                'clusterLabel_y': 'c_clL', 'clusterRadius_y': 'c_clR',
                'label_y': 'c_L', 'distance_y': 'c_D'}
    toKeep = ['id', 'j_clL', 'j_clR', 'j_L', 'j_D', 'c_clL', 'c_clR', 'c_L', 'c_D']
    print("### Diff\n")
    print(diff.rename(columns=toRename)[toKeep])
    return diff

def getModelDf(path):
    assert os.path.isfile(path), "file '%s' not found." % path
    df = pd.read_csv(filepath_or_buffer=path)
    df['id'] = df['#id'].astype(int)
    msum = df['matches'].sum()
    df['matches_p'] = df['matches'].astype(float) / msum
    return df.drop(['#id', 'time'], axis=1)  # .sort_values('meanDistance')

def compareModelDf(a, b, path=None):
    if (path is not None):
        assert os.path.isdir(path), "dir '%s' not found." % path
    # figKArgs = {papertype: 'a4', }
    # + [ 'matches', 'meanDistance', 'radius']
    toDrop = ['id', 'label', 'category']
    aCl = a.drop(toDrop, axis=1)
    bCl = b.drop(toDrop, axis=1)
    c = aCl - bCl
    d = c[c != 0].abs().dropna(axis=1, how='all')
    #
    #     d = c[c != 0]
    #     d = d.abs().dropna(axis=1, how='all')
    #     dmin = d.min().min()
    #     g = plt.pcolor(d, norm=LogNorm(vmin=dmin, vmax=1), vmin=dmin, vmax=1)
    #     plt.colorbar()
    #     plt.show()
    fig, (ax0, ax1) = plt.subplots(2, 1)
    ax0.set_title('Full model')
    ax0.pcolor(aCl)
    ax1.pcolor(bCl)
    if (path is not None):
        fig.set_size_inches(figsize)
        fig.savefig(path + 'model-full.png', dpi=dpi, bbox_inches='tight')
        print('saving', path + 'model-full.png')
    # 
    fig, (ax0, ax1) = plt.subplots(2, 1)
    ax0.set_title('Full diff')
    ax0.pcolor(c)
    ax1.pcolor(d)
    if (path is not None):
        fig.set_size_inches(figsize)
        fig.savefig(path + 'model-diff.png', dpi=dpi, bbox_inches='tight')
        print('saving', path + 'model-diff.png')
    # 
    m = pd.merge(a, b, on='id', how='left')
    diff = m[m['matches_x'] != m['matches_y']]
    diffIds = diff['id']
    e = a[a['id'].isin(diffIds)].drop(toDrop, axis=1)
    f = b[b['id'].isin(diffIds)].drop(toDrop, axis=1)
    g = e - f
    h = g[g != 0].abs().dropna(axis=1, how='all')
    fig, (ax0) = plt.subplots(1, 1)
    ax0.set_title('Merge ==id, !=matches')
    ax0.pcolor(h)
    # ax1.pcolor(f)
    if (path is not None):
        fig.set_size_inches(figsize)
        fig.savefig(path + 'model-diff-matches.png', dpi=dpi, bbox_inches='tight')
        print('saving', path + 'model-diff-matches.png')
    return d


def main(
    examplesFN='datasets/test.csv',
    matchesFN='out/matches.csv',
    minasMatches='out/og/2020-07-20T12-18-21.758/matches.csv',
    modelFN='datasets/model-clean.csv',
    mfogModelFN='out/model.csv',
    outputDir='out/',
):
    print(
        __file__ + '\n' +
        '\texamplesFN = %s\n' % examplesFN +
        '\tmatchesFN = %s\n' % matchesFN +
        '\tminasMatches = %s\n' % minasMatches +
        '\tmodelFN = %s\n' % modelFN +
        '\tmfogModelFN = %s\n' % mfogModelFN
    )
    examplesDf = getExamplesDf(examplesFN)
    countPerClass = examplesDf.groupby('class').count()[['id']]
    print("Count per class")
    print(countPerClass)

    mfogDF = getMatchesDf(matchesFN)
    minasDF = getMatchesDf(minasMatches)
    # ogdf = getOriginalMatchesDf('../../out/minas-og/2020-07-20T12-21-54.755/results')
    diffMinasMfog(examplesDf, minasDF, mfogDF, outputDir)

    modelDF = getModelDf(modelFN)
    # minasFiModDF = getModelDf('../../out/minas-og/2020-07-22T01-19-11.984/model/653457_final.csv')
    mfogModelDF = getModelDf(mfogModelFN)
    compareModelDf(modelDF, mfogModelDF, outputDir)

    # d['meanDistance'].abs().sum()
    # d.min().min()
    # newIni = getModelDf('../../out/minas-og/2020-07-22T21-46-01.167/model/0_initial.csv')
    # newFin = getModelDf('../../out/minas-og/2020-07-22T21-46-01.167/model/653457_final.csv')
    # compareModelDf(modelDF, newIni)
    # compareModelDf(newIni, newFin)
    # compareModelDf(mfogModelDF, newIni)

if __name__ == "__main__":
    params = [
        'examplesFN',
        'matchesFN',
        'minasMatches',
        'modelFN',
        'mfogModelFN',
        'outputDir',
    ]
    main(**dict(zip(params, sys.argv[1:])))
    print('')
