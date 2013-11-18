#ifndef __NOUVEAU_SYNC_H__
#define __NOUVEAU_SYNC_H__

#include "nv_include.h"

#include "misync.h"
#include "misyncshm.h"
#include "misyncstr.h"

#define wrap(priv, parn, name, func) {                                         \
    priv->name = parn->name;                                                   \
    parn->name = func;                                                         \
}

#define unwrap(priv, parn, name) {                                             \
    if (priv && priv->name)                                                    \
	parn->name = priv->name;                                               \
}

#define swap(priv, parn, name) {                                               \
    void *tmp = priv->name;                                                    \
    priv->name = parn->name;                                                   \
    parn->name = tmp;                                                          \
}

#endif
