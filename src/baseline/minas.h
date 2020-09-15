#ifndef _MINAS_H
#define _MINAS_H

#include <stdio.h>

#include "./base.h"

Cluster* clustering(Params *params, Example trainingSet[], unsigned int trainingSetSize, unsigned int initalId);
Model *training(Params *params);
Match *identify(Params *params, Model *model, Example *example, Match *match);
void noveltyDetection(Params *params, Model *model, Example *unknowns, size_t unknownsSize);
void minasOnline(Params *params, Model *model);

#endif // !_MINAS_H