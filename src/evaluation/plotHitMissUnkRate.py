#!/usr/bin/python3
# coding: utf-8

import sys
import os.path
import numpy as np
import pandas as pd
import matplotlib
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm

dpi = 300
figsize = (1920 / dpi, 1080 / dpi)

def plotHitMissUnkRate(df, assignment, off, plotSavePath=None, title=None):
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
    # print(df.describe())
    # print(df)
    # 
    # if path is not None:
    #     path = path + title + '_hits.png'
    if title is None:
        title = 'Hit Miss Unk'
    stackLabels = ['%s %1.3f' % (a, df['d_' + a].iloc[-1]) for a in ['mis', 'unk', 'hit']]
    # ax, _, _ = plt.stackplot(df['id'], df['d_mis'], df['d_unk'], df['d_hit'], labels=stackLabels, alpha=0.8)
    # ax, _, _ = plt.plot(df['id'], df[['d_mis', 'd_unk', 'd_hit']], alpha=0.8)
    # ax = ax.axes
    # colors=['r', 'g', 'b']
    ax = df[['d_mis', 'd_unk', 'd_hit' ]].plot(title=title, figsize=figsize, legend=False)
    #
    labelSet = [ (r.Index, r.id) for r in df[['id', 'label']].groupby('label').min().sort_values(by=['id']).itertuples() ]
    lastLabel = len(labelSet) - 1
    prevXcoord = 0
    last = len(df) - 1
    for idx, (l, i) in enumerate(labelSet):
        if (l not in off) and (idx == 0 or idx == lastLabel or i - prevXcoord > last / 10):
            ax.annotate(l, xy=(i, 1.06), xytext=(-10, 0), textcoords='offset pixels', fontsize='xx-small')
            prevXcoord = i
    xcoords = [ i for (l, i) in labelSet if (l not in off) ]
    ax.vlines(x=xcoords, ymin=-0.04, ymax=1.04, colors='gray', ls='--', lw=0.5, label='vline_multiple')
    ax.get_xaxis().set_major_formatter(matplotlib.ticker.EngFormatter())
    #
    allPos = []
    offset = 0.04
    for n, c in zip(['mis', 'unk', 'hit'],
                    ['b', 'r', 'g']):
        v = df['d_' + n].iloc[-1]
        allPos += [[v + offset, n, v, c]]
    for a in allPos:
        for b in allPos:
            if a == b: continue
            diff = a[0] - b[0]
            if diff > offset or diff < -offset: continue
            if diff > 0: # a is bigger
                b[0] -= 2* offset
            else:
                a[0] -= 2* offset
    for pos, name, y, color in allPos:
        ax.annotate(name + ' %1.3f' % y, xy=(last, y), xytext=(last * 0.9, pos), fontsize='small', color=color)
    #
    if (plotSavePath is not None):
        plt.savefig(plotSavePath, dpi=dpi, bbox_inches='tight')
        print('saving', plotSavePath)
    else:
        print('showing')
        plt.show()
    return (df, labelSet, xcoords, allPos)
