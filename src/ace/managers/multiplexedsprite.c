/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ace/managers/multiplexedsprite.h>
#include <string.h>
#include <ace/managers/blit.h>
#include <ace/managers/memory.h>
#include <ace/managers/sprite.h>
#include <ace/managers/system.h>
#include <ace/utils/custom.h>
#include <ace/utils/sprite.h>

typedef struct _tMultiplexedChannel {
	tMultiplexedSprite *pSprite;
	tCopBlock *pCopBlock;
	UWORD uwRawCopPos;
} tMultiplexedChannel;

static const tView *s_pView;
static tMultiplexedChannel s_pChannels[HARDWARE_SPRITE_CHANNEL_COUNT];
static ULONG *s_pBlankSprite;
static UBYTE s_isOwningBlankSprite;
static tCopBlock *s_pInitialClearCopBlock;

/** Return an initialized element, or NULL for an invalid public API argument. */
static tMultiplexedSpriteElement *spriteGetElement(
	tMultiplexedSprite *pSprite, UBYTE ubIndex
) {
	if(!pSprite || ubIndex >= pSprite->ubSpriteCount) {
		return 0;
	}
	return pSprite->pMultiplexedSpriteElement[ubIndex];
}

void spriteMultiplexedManagerCreate(
	const tView *pView, UWORD uwRawCopPos, ULONG pBlankSprite[1]
) {
	if(!pView || !pView->pCopList) {
		return;
	}
	systemUse();
	s_pView = pView;
	s_isOwningBlankSprite = !pBlankSprite;
	s_pBlankSprite = pBlankSprite ? pBlankSprite : memAllocChipClear(4);
	if(!s_pBlankSprite) {
		s_pView = 0;
		systemUnuse();
		return;
	}
	for(UBYTE i = 0; i < HARDWARE_SPRITE_CHANNEL_COUNT; ++i) {
		s_pChannels[i] = (tMultiplexedChannel){
			.uwRawCopPos = uwRawCopPos + 2 * i
		};
	}
	if(pView->pCopList->ubMode == COPPER_MODE_BLOCK) {
		s_pInitialClearCopBlock = spriteDisableInCopBlockMode(
			pView->pCopList, 0xFF, s_pBlankSprite
		);
		if(!s_pInitialClearCopBlock) {
			if(s_isOwningBlankSprite) {
				memFree(s_pBlankSprite, 4);
			}
			s_pBlankSprite = 0;
			s_pView = 0;
		}
	}
	else {
		s_pInitialClearCopBlock = 0;
		spriteDisableInCopRawMode(
			pView->pCopList, 0xFF, uwRawCopPos, s_pBlankSprite
		);
	}
	systemUnuse();
}

void spriteMultiplexedManagerDestroy(void) {
	if(!s_pView) {
		return;
	}
	systemUse();
	for(UBYTE i = 0; i < HARDWARE_SPRITE_CHANNEL_COUNT; ++i) {
		spriteMultiplexedRemove(s_pChannels[i].pSprite);
		if(s_pChannels[i].pCopBlock) {
			copBlockDestroy(s_pView->pCopList, s_pChannels[i].pCopBlock);
		}
	}
	if(s_pInitialClearCopBlock) {
		copBlockDestroy(s_pView->pCopList, s_pInitialClearCopBlock);
	}
	if(s_isOwningBlankSprite) {
		memFree(s_pBlankSprite, 4);
	}
	memset(s_pChannels, 0, sizeof(s_pChannels));
	s_pInitialClearCopBlock = 0;
	s_pBlankSprite = 0;
	s_pView = 0;
	systemUnuse();
}

tMultiplexedSprite *spriteMultiplexedAdd(
	UBYTE ubChannelIndex, UBYTE uwSpriteHeight,
	UBYTE ubNumberOfMultiplexedSprites
) {
	if(!s_pView || !s_pBlankSprite || !uwSpriteHeight ||
		!ubNumberOfMultiplexedSprites ||
		ubChannelIndex >= HARDWARE_SPRITE_CHANNEL_COUNT ||
		s_pChannels[ubChannelIndex].pSprite) {
		return 0;
	}
	systemUse();
	tMultiplexedSprite *pSprite = memAllocFastClear(sizeof(*pSprite));
	if(!pSprite) {
		systemUnuse();
		return 0;
	}
	pSprite->ubChannelIndex = ubChannelIndex;
	pSprite->isEnabled = 1;
	pSprite->ubSpriteCount = ubNumberOfMultiplexedSprites;
	pSprite->uwMaxHeight = uwSpriteHeight;
	pSprite->uwTotalHeight =
		(uwSpriteHeight + 1UL) * ubNumberOfMultiplexedSprites + 1;
	pSprite->pMultiplexedSpriteElement = memAllocFastClear(
		sizeof(*pSprite->pMultiplexedSpriteElement) * pSprite->ubSpriteCount
	);
	pSprite->pOrder = memAllocFast(pSprite->ubSpriteCount);
	if(!pSprite->pMultiplexedSpriteElement || !pSprite->pOrder) {
		goto fail;
	}
	for(UWORD i = 0; i < pSprite->ubSpriteCount; ++i) {
		tMultiplexedSpriteElement *pElement = memAllocFastClear(sizeof(*pElement));
		if(!pElement) {
			goto fail;
		}
		pSprite->pMultiplexedSpriteElement[i] = pElement;
		pElement->uwHeight = uwSpriteHeight;
		pElement->pBufferOffset[0] = pElement->pBufferOffset[1] = 0xFFFF;
		pSprite->pOrder[i] = i;
	}
	for(UBYTE i = 0; i < 2; ++i) {
		pSprite->pDmaBitmap[i] = bitmapCreate(
			16, pSprite->uwTotalHeight, 2, BMF_CLEAR | BMF_INTERLEAVED
		);
		if(!pSprite->pDmaBitmap[i]) {
			goto fail;
		}
	}
	pSprite->pBufferIdentity = s_pView->pCopList->pBackBfr;
	pSprite->pBitmap = pSprite->pDmaBitmap[0];
	tMultiplexedChannel *pChannel = &s_pChannels[ubChannelIndex];
	if(s_pView->pCopList->ubMode == COPPER_MODE_BLOCK &&
		!pChannel->pCopBlock) {
		pChannel->pCopBlock = copBlockCreate(s_pView->pCopList, 2, 0, 1);
		if(!pChannel->pCopBlock) {
			goto fail;
		}
	}
	pChannel->pSprite = pSprite;
	systemUnuse();
	return pSprite;
fail:
	spriteMultiplexedRemove(pSprite);
	systemUnuse();
	return 0;
}

void spriteMultiplexedRemove(tMultiplexedSprite *pSprite) {
	if(!pSprite) {
		return;
	}
	systemUse();
	tMultiplexedChannel *pChannel = &s_pChannels[pSprite->ubChannelIndex];
	if(pChannel->pSprite == pSprite) {
		pChannel->pSprite = 0;
		// Retain a blank pointer block until manager destruction/reuse.
		spriteMultiplexedProcessChannel(pSprite->ubChannelIndex);
	}
	for(UBYTE i = 0; i < 2; ++i) {
		if(pSprite->pDmaBitmap[i]) {
			bitmapDestroy(pSprite->pDmaBitmap[i]);
		}
	}
	if(pSprite->pMultiplexedSpriteElement) {
		for(UWORD i = 0; i < pSprite->ubSpriteCount; ++i) {
			if(pSprite->pMultiplexedSpriteElement[i]) {
				memFree(pSprite->pMultiplexedSpriteElement[i],
					sizeof(tMultiplexedSpriteElement));
			}
		}
		memFree(pSprite->pMultiplexedSpriteElement,
			sizeof(*pSprite->pMultiplexedSpriteElement) * pSprite->ubSpriteCount);
	}
	if(pSprite->pOrder) {
		memFree(pSprite->pOrder, pSprite->ubSpriteCount);
	}
	memFree(pSprite, sizeof(*pSprite));
	systemUnuse();
}

void spriteMultiplexedSetElement(
	tMultiplexedSprite *pSprite, UBYTE ubIndex, UWORD uwHeight,
	UBYTE isEnabled, UBYTE isAttached
) {
	tMultiplexedSpriteElement *pElement = spriteGetElement(pSprite, ubIndex);
	if(!pElement || !uwHeight || uwHeight > pSprite->uwMaxHeight ||
		(pElement->pBitmap && uwHeight > pElement->pBitmap->Rows)) {
		return;
	}
	spriteMultiplexedSetHeight(pSprite, ubIndex, uwHeight);
	spriteMultiplexedSetEnabled(pSprite, ubIndex, isEnabled);
	spriteMultiplexedSetAttached(pSprite, ubIndex, isAttached);
}

void spriteMultiplexedSetBitmap(
	tMultiplexedSprite *pSprite, UBYTE ubIndex, tBitMap *pBitmap
) {
	tMultiplexedSpriteElement *pElement = spriteGetElement(pSprite, ubIndex);
	if(!pElement || !pBitmap || !(pBitmap->Flags & BMF_INTERLEAVED) ||
		pBitmap->Depth != 2 || bitmapGetByteWidth(pBitmap) != 2 ||
		!pBitmap->Planes[0] || pBitmap->Planes[1] != pBitmap->Planes[0] + 2 ||
		pBitmap->BytesPerRow != 4 || !pBitmap->Rows ||
		pBitmap->Rows > pSprite->uwMaxHeight) {
		return;
	}
	pElement->pBitmap = pBitmap;
	pElement->uwHeight = pBitmap->Rows;
	// Calling again with the same bitmap explicitly refreshes edited pixels.
	pElement->isBitmapToBeUpdated = 3;
}

void spriteMultiplexedSpriteSetPos(
	tMultiplexedSprite *pSprite, UBYTE ubIndex, WORD wX, WORD wY
) {
	tMultiplexedSpriteElement *pElement = spriteGetElement(pSprite, ubIndex);
	if(pElement) {
		if(pElement->wY != wY) {
			pSprite->isHeaderToBeUpdated = 1;
		}
		pElement->wX = wX;
		pElement->wY = wY;
	}
}

void spriteMultiplexedSetEnabled(
	tMultiplexedSprite *pSprite, UBYTE ubIndex, UBYTE isEnabled
) {
	tMultiplexedSpriteElement *pElement = spriteGetElement(pSprite, ubIndex);
	if(pElement) {
		pElement->isEnabled = !!isEnabled;
	}
}

void spriteMultiplexedSetAttached(
	tMultiplexedSprite *pSprite, UBYTE ubIndex, UBYTE isAttached
) {
	tMultiplexedSpriteElement *pElement = spriteGetElement(pSprite, ubIndex);
	if(pElement) {
		pElement->isAttached = !!isAttached && (pSprite->ubChannelIndex & 1);
	}
}

void spriteMultiplexedSetHeight(
	tMultiplexedSprite *pSprite, UBYTE ubIndex, UWORD uwHeight
) {
	tMultiplexedSpriteElement *pElement = spriteGetElement(pSprite, ubIndex);
	if(pElement && uwHeight && uwHeight <= pSprite->uwMaxHeight &&
		(!pElement->pBitmap || uwHeight <= pElement->pBitmap->Rows) &&
		pElement->uwHeight != uwHeight) {
		pElement->uwHeight = uwHeight;
		pElement->isBitmapToBeUpdated = 3;
	}
}

void spriteMultiplexedRequestMetadataUpdate(
	tMultiplexedSprite *pSprite, UBYTE ubIndex
) {
	tMultiplexedSpriteElement *pElement = spriteGetElement(pSprite, ubIndex);
	if(pElement) {
		pSprite->isHeaderToBeUpdated = 1;
		pElement->isBitmapToBeUpdated = 3;
	}
}

/** Keep a persistent Y/index order; insertion sort exploits temporal coherence. */
static void spriteSort(tMultiplexedSprite *pSprite) {
	for(UWORD i = 1; i < pSprite->ubSpriteCount; ++i) {
		UBYTE ubIndex = pSprite->pOrder[i];
		WORD wY = pSprite->pMultiplexedSpriteElement[ubIndex]->wY;
		UWORD j = i;
		while(j) {
			UBYTE ubPrevious = pSprite->pOrder[j - 1];
			WORD wPreviousY = pSprite->pMultiplexedSpriteElement[ubPrevious]->wY;
			if(wPreviousY < wY || (wPreviousY == wY && ubPrevious < ubIndex)) {
				break;
			}
			pSprite->pOrder[j] = ubPrevious;
			--j;
		}
		pSprite->pOrder[j] = ubIndex;
	}
	pSprite->isHeaderToBeUpdated = 0;
}

/** One packed two-plane row is four bytes; constant-size copies let 68000 GCC
 * emit a longword move rather than the toolchain's general byte-copy loop.
 */
static void spriteCopyRows(UBYTE *pDest, const UBYTE *pSource, UWORD uwRows) {
	while(uwRows--) {
		memcpy(pDest, pSource, 4);
		pDest += 4;
		pSource += 4;
	}
}

void spriteMultiplexedProcess(tMultiplexedSprite *pSprite) {
	if(!pSprite || !s_pView) {
		return;
	}
	if(pSprite->isHeaderToBeUpdated) {
		spriteSort(pSprite);
	}
	UBYTE ubBuffer = s_pView->pCopList->pBackBfr != pSprite->pBufferIdentity;
	UBYTE ubMask = 1 << ubBuffer;
	pSprite->pBitmap = pSprite->pDmaBitmap[ubBuffer];
	UWORD *pData = (UWORD *)pSprite->pBitmap->Planes[0];
	UWORD uwOffset = 0;
	// PAL fetches initial controls on line 25. Use the PAL-safe lower bound
	// on NTSC too: a skipped VSTART would strand the rest of the DMA chain.
	LONG lNextY = 26;
	UBYTE isBlitterReady = 0;
	for(UWORD i = 0; i < pSprite->ubSpriteCount; ++i) {
		tMultiplexedSpriteElement *pElement =
			pSprite->pMultiplexedSpriteElement[pSprite->pOrder[i]];
		LONG lX = (LONG)s_pView->ubPosX - 1 + pElement->wX;
		LONG lY = (LONG)s_pView->ubPosY + pElement->wY;
		LONG lStop = lY + pElement->uwHeight;
		if(!pSprite->isEnabled || !pElement->isEnabled || !pElement->pBitmap ||
			!pElement->uwHeight || pElement->uwHeight > pSprite->uwMaxHeight ||
			pElement->uwHeight > pElement->pBitmap->Rows ||
			lX < 0 || lX > 511 || lY < lNextY || lStop > 511) {
			pElement->pBufferOffset[ubBuffer] = 0xFFFF;
			continue;
		}
		tHardwareSpriteHeader *pHeader = (void *)&pData[2UL * uwOffset];
		pHeader->uwRawPos = ((lY & 255) << 8) | ((lX >> 1) & 255);
		pHeader->uwRawCtl = ((lStop & 255) << 8) |
			((pElement->isAttached && (pSprite->ubChannelIndex & 1)) << 7) |
			(((lY >> 8) & 1) << 2) | (((lStop >> 8) & 1) << 1) | (lX & 1);
		if((pElement->isBitmapToBeUpdated & ubMask) ||
			pElement->pBufferOffset[ubBuffer] != uwOffset) {
			if(!isBlitterReady) {
				// Source pixels may come from a caller's asynchronous blit.
				blitWait();
				isBlitterReady = 1;
			}
			spriteCopyRows((UBYTE *)&pData[2UL * (uwOffset + 1)],
				pElement->pBitmap->Planes[0], pElement->uwHeight);
			pElement->isBitmapToBeUpdated &= ~ubMask;
		}
		pElement->pBufferOffset[ubBuffer] = uwOffset;
		uwOffset += pElement->uwHeight + 1;
		// The VSTOP line fetches the next control pair, not next sprite pixels.
		lNextY = lStop + 1;
	}
	pData[2UL * uwOffset] = 0;
	pData[2UL * uwOffset + 1] = 0;
	pSprite->pPreparedBuffer = s_pView->pCopList->pBackBfr;
}

void spriteMultiplexedProcessChannel(UBYTE ubChannelIndex) {
	if(!s_pView || ubChannelIndex >= HARDWARE_SPRITE_CHANNEL_COUNT) {
		return;
	}
	tMultiplexedChannel *pChannel = &s_pChannels[ubChannelIndex];
	tMultiplexedSprite *pSprite = pChannel->pSprite;
	ULONG ulAddress = (ULONG)s_pBlankSprite;
	if(pSprite) {
		// Never publish a stream prepared for the other copper buffer.
		if(pSprite->pPreparedBuffer != s_pView->pCopList->pBackBfr) {
			spriteMultiplexedProcess(pSprite);
		}
		if(pSprite->isEnabled) {
			ulAddress = (ULONG)pSprite->pBitmap->Planes[0];
		}
	}
	if(s_pView->pCopList->ubMode == COPPER_MODE_BLOCK) {
		if(pChannel->pCopBlock) {
			pChannel->pCopBlock->uwCurrCount = 0;
			copMove(s_pView->pCopList, pChannel->pCopBlock,
				&g_pSprFetch[ubChannelIndex].uwHi, ulAddress >> 16);
			copMove(s_pView->pCopList, pChannel->pCopBlock,
				&g_pSprFetch[ubChannelIndex].uwLo, ulAddress & 0xFFFF);
		}
	}
	else {
		tCopCmd *pList = &s_pView->pCopList->pBackBfr->pList[
			pChannel->uwRawCopPos];
		copSetMoveVal(&pList[0].sMove, ulAddress >> 16);
		copSetMoveVal(&pList[1].sMove, ulAddress & 0xFFFF);
	}
}
