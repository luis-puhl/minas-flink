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
    labelSet = set()
    xcoords = []
    prevLen = len(off)
    for i, l in zip(df.index, df['label']):
        labelSet.add(l)
        if len(labelSet) > prevLen:
            prevLen = len(labelSet)
            xcoords += [i]
    # 
    # if path is not None:
    #     path = path + title + '_hits.png'
    if title is None:
        title = 'Hit Miss Unk'
    ax = df[['d_hit', 'd_mis', 'd_unk' ]].plot(title=title, figsize=figsize, legend=False)
    # ax = df[['d_hit', 'd_unk' ]].plot(title=title, figsize=figsize)
    ax.vlines(x=xcoords, ymin=-0.05, ymax=1.05, colors='gray', ls='--', lw=0.5, label='vline_multiple')
    ax.get_xaxis().set_major_formatter(matplotlib.ticker.EngFormatter())
    #
    last = len(df) - 1
    annotate = lambda name, y: ax.annotate(name + ' %1.3f' % y, xy=(last, y),
        xytext=(last * 0.9, y + 0.05), fontsize='small')
    annotate('hit', df['d_hit'].iloc[-1])
    annotate('mis', df['d_mis'].iloc[-1])
    annotate('unk', df['d_unk'].iloc[-1])
    #
    if (plotSavePath is not None):
        plt.savefig(plotSavePath, dpi=dpi, bbox_inches='tight')
        print('saving', plotSavePath)
    else:
        plt.show()
    return df
