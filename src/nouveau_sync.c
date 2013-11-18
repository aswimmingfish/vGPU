/*
 * Copyright 2013 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Ben Skeggs <bskeggs@redhat.com>
 */

#include "nouveau_sync.h"

static DevPrivateKeyRec nouveau_syncobj_key;

struct nouveau_syncobj {
	SyncFenceSetTriggeredFunc SetTriggered;
};

#define nouveau_syncobj(fence)                                                 \
	dixLookupPrivate(&(fence)->devPrivates, &nouveau_syncobj_key)

struct nouveau_syncctx {
	SyncScreenCreateFenceFunc CreateFence;
};

#define nouveau_syncctx(screen) ({                                             \
	ScrnInfoPtr scrn = xf86ScreenToScrn(screen);                           \
	NVPtr pNv = NVPTR(scrn);                                               \
	pNv->sync;                                                             \
})

static void
nouveau_syncobj_flush(SyncFence *fence)
{
	struct nouveau_syncobj *pobj = nouveau_syncobj(fence);
	ScrnInfoPtr scrn = xf86ScreenToScrn(fence->pScreen);
	NVPtr pNv = NVPTR(scrn);
	SyncFenceFuncsPtr func = &fence->funcs;

	nouveau_pushbuf_kick(pNv->pushbuf, pNv->channel);
	if (pNv->ce_pushbuf)
		nouveau_pushbuf_kick(pNv->ce_pushbuf, pNv->ce_channel);

	swap(pobj, func, SetTriggered);
	func->SetTriggered(fence);
	swap(pobj, func, SetTriggered);
}

static void
nouveau_syncobj_new(ScreenPtr screen, SyncFence *fence, Bool triggered)
{
	struct nouveau_syncctx *priv = nouveau_syncctx(screen);
	struct nouveau_syncobj *pobj = nouveau_syncobj(fence);
	SyncScreenFuncsPtr sync = miSyncGetScreenFuncs(screen);
	SyncFenceFuncsPtr func = &fence->funcs;

	swap(priv, sync, CreateFence);
	sync->CreateFence(screen, fence, triggered);
	swap(priv, sync, CreateFence);

	wrap(pobj, func, SetTriggered, nouveau_syncobj_flush);
}

void
nouveau_sync_fini(ScreenPtr screen)
{
	struct nouveau_syncctx *priv = nouveau_syncctx(screen);
	SyncScreenFuncsPtr sync = miSyncGetScreenFuncs(screen);
	ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
	NVPtr pNv = NVPTR(scrn);

	unwrap(priv, sync, CreateFence);

	pNv->sync = NULL;
	free(priv);
}

Bool
nouveau_sync_init(ScreenPtr screen)
{
	ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
	NVPtr pNv = NVPTR(scrn);
	struct nouveau_syncctx *priv;
	SyncScreenFuncsPtr sync;

	priv = pNv->sync = calloc(1, sizeof(*priv));
	if (!priv)
		return FALSE;

	if (!dixPrivateKeyRegistered(&nouveau_syncobj_key) &&
	    !dixRegisterPrivateKey(&nouveau_syncobj_key, PRIVATE_SYNC_FENCE,
				   sizeof(struct nouveau_syncobj)))
		return FALSE;

	if (!miSyncShmScreenInit(screen))
		return FALSE;
	sync = miSyncGetScreenFuncs(screen);
	wrap(priv, sync, CreateFence, nouveau_syncobj_new);
	return TRUE;
}
