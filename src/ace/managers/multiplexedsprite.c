/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ace/managers/multiplexedsprite.h>
#include <ace/managers/blit.h>
#include <ace/managers/sprite.h>
#include <ace/macros.h>
#include <ace/generic/screen.h>
#include <ace/managers/system.h>
#include <ace/managers/mouse.h>
#include <ace/managers/log.h>
#include <ace/utils/custom.h>
#include <ace/utils/sprite.h>



#define SPRITE_WIDTH 16
#define TWOBPP_BYTEWIDTH 2


// See if can be only in sprite.c/sprite.h
typedef struct tSpriteChannel {
	tMultiplexedSprite *pFirstSprite; ///< First sprite on the chained list in channel.
	tCopBlock *pCopBlock;
	UWORD uwRawCopPos; ///< Offset for channel's first sprite fetch copper cmd.
	UBYTE ubCopperRegenCount;
} tSpriteChannel;

static const tView *s_pView;
static tSpriteChannel s_pChannelsData[HARDWARE_SPRITE_CHANNEL_COUNT];
static ULONG *s_pBlankSprite;

static void spriteChannelRequestCopperUpdate(tSpriteChannel *pChannel) {
	pChannel->ubCopperRegenCount = 2; // for front/back buffers in raw mode
}
/// end See if can be only in sprite.c/sprite.h


tMultiplexedSprite *spriteMultiplexedAdd(UBYTE ubChannelIndex, UWORD uwSpriteTotalHeight,UBYTE ubNumberOfMultiplexedSprites) {
	systemUse();
	tMultiplexedSprite *pMultiplexedSprite = memAllocFastClear(sizeof(*pMultiplexedSprite));
	pMultiplexedSprite->ubChannelIndex = ubChannelIndex;
	pMultiplexedSprite->isEnabled = 1;
	pMultiplexedSprite->ubSpriteCount = ubNumberOfMultiplexedSprites;
	pMultiplexedSprite->uwTotalHeight = (1 + uwSpriteTotalHeight) * ubNumberOfMultiplexedSprites + 1; // added "VSTART, HSTART, VSTOP for each sprite" + 1 "End of sprite data"
	pMultiplexedSprite->pMultiplexedSpriteElement = (tMultiplexedSpriteElement **)memAllocFastClear(sizeof(tMultiplexedSpriteElement*) * ubNumberOfMultiplexedSprites);
	pMultiplexedSprite->pBitmap = bitmapCreate(
        SPRITE_WIDTH, pMultiplexedSprite->uwTotalHeight,
        2, BMF_CLEAR | BMF_INTERLEAVED);

	tSpriteChannel *pChannel = &s_pChannelsData[ubChannelIndex];
	if(pChannel->pFirstSprite) {
		logWrite("ERR: Sprite channel %hhu is already used\n", ubChannelIndex);
	}
	else {
		spriteChannelRequestCopperUpdate(pChannel);
		pChannel->pFirstSprite = pMultiplexedSprite;

		if(s_pView->pCopList->ubMode == COPPER_MODE_BLOCK) {
#if defined(ACE_DEBUG)
			if(pChannel->pCopBlock) {
				logWrite("ERR: Sprite channel %hhu already has copBlock\n", ubChannelIndex);
				systemUnuse();
				return 0;
			}
#endif
			pChannel->pCopBlock = copBlockCreate(s_pView->pCopList, 2, 0, 1);
		}
	}
	systemUnuse();
	return pMultiplexedSprite;
}

void spriteMultiplexedSetElement(tMultiplexedSprite *pMultiplexedSprite, UBYTE ubSpriteIndex, UWORD uwSpriteHeight, UBYTE isEnabled, UBYTE isAttached) {
	pMultiplexedSprite->pMultiplexedSpriteElement[ubSpriteIndex] = memAllocFastClear(sizeof(tMultiplexedSpriteElement));
	pMultiplexedSprite->pMultiplexedSpriteElement[ubSpriteIndex]->uwHeight = uwSpriteHeight;
	pMultiplexedSprite->pMultiplexedSpriteElement[ubSpriteIndex]->isEnabled = isEnabled;
	pMultiplexedSprite->pMultiplexedSpriteElement[ubSpriteIndex]->isAttached = isAttached;
	pMultiplexedSprite->pMultiplexedSpriteElement[ubSpriteIndex]->isHeaderToBeUpdated = 1;
}

void spriteMultiplexedRemove(tMultiplexedSprite *pMultiplexedSprite) {
	systemUse();
	tSpriteChannel *pChannel = &s_pChannelsData[pMultiplexedSprite->ubChannelIndex];

	if(pChannel->pFirstSprite == pMultiplexedSprite) {
		pChannel->pFirstSprite = 0;
		spriteChannelRequestCopperUpdate(pChannel);

		if(s_pView->pCopList->ubMode == COPPER_MODE_BLOCK) {
			tCopBlock *pCopBlock = pChannel->pCopBlock;
			if(pCopBlock) {
				copBlockDestroy(s_pView->pCopList, pCopBlock);
				pChannel->pCopBlock = 0;
			}
		}
	}

	memFree(pMultiplexedSprite, sizeof(*pMultiplexedSprite));
	systemUnuse();
}


void spriteMultiplexedSetBitmap(tMultiplexedSprite *pMultiplexedSprite, UBYTE ubSpriteIndex, tBitMap *pBitmap) {
if(!(pBitmap->Flags & BMF_INTERLEAVED) || pBitmap->Depth != 2) {
		logWrite(
			"ERR: Sprite channel %hhu bitmap %p isn't interleaved 2BPP\n",
			pMultiplexedSprite->ubChannelIndex, pBitmap
		);
		return;
	}

	UBYTE ubByteWidth = bitmapGetByteWidth(pBitmap);
	if(ubByteWidth != 2) {
		logWrite(
			"ERR: Unsupported sprite width: %hhu, expected 16\n",
			ubByteWidth * 8
		);
		return;
	}

	pMultiplexedSprite->pMultiplexedSpriteElement[ubSpriteIndex]->pBitmap = pBitmap;
	pMultiplexedSprite->pMultiplexedSpriteElement[ubSpriteIndex]->isBitmapToBeUpdated = 1;
	spriteMultiplexedSetHeight(pMultiplexedSprite,ubSpriteIndex, pBitmap->Rows);

	tSpriteChannel *pChannel = &s_pChannelsData[pMultiplexedSprite->ubChannelIndex];
	spriteChannelRequestCopperUpdate(pChannel);
}

//after

void spriteMultiplexedSpriteSetPos(tMultiplexedSprite *pMultiplexedSprite, UBYTE ubSpriteIndex, WORD wX, WORD wY) {
	pMultiplexedSprite->pMultiplexedSpriteElement[ubSpriteIndex]->wX = wX;
	pMultiplexedSprite->pMultiplexedSpriteElement[ubSpriteIndex]->wY = wY;
	pMultiplexedSprite->pMultiplexedSpriteElement[ubSpriteIndex]->isHeaderToBeUpdated = 1;
}

void spriteMultiplexedSetEnabled(tMultiplexedSprite *pMultiplexedSprite, UBYTE ubSpriteIndex, UBYTE isEnabled) {
	pMultiplexedSprite->pMultiplexedSpriteElement[ubSpriteIndex]->isEnabled = isEnabled;
	pMultiplexedSprite->pMultiplexedSpriteElement[ubSpriteIndex]->isHeaderToBeUpdated = 1;
	//TODO spriteChannelRequestCopperUpdate(pChannel);
}

void spriteMultiplexedSetAttached(tMultiplexedSprite *pMultiplexedSprite,  UBYTE spriteIndex, UBYTE isAttached) {
	pMultiplexedSprite->pMultiplexedSpriteElement[spriteIndex]->isAttached = isAttached;
	pMultiplexedSprite->pMultiplexedSpriteElement[spriteIndex]->isHeaderToBeUpdated = 1;
}

void spriteMultiplexedRequestMetadataUpdate(tMultiplexedSprite *pMultiplexedSprite, UBYTE spriteIndex) {
	pMultiplexedSprite->pMultiplexedSpriteElement[spriteIndex]->isHeaderToBeUpdated = 1;
}
//after

void spriteMultiplexedProcess(tMultiplexedSprite *pMultiplexedSprite) {
	UWORD offset = 0;
	UWORD offsetHeight = 1;

	for (UBYTE i = 0; i < pMultiplexedSprite->ubSpriteCount; i++) {
		tMultiplexedSpriteElement *pSpriteElement = pMultiplexedSprite->pMultiplexedSpriteElement[i];
		// Update header (X,Y, attached, height)
		if (pSpriteElement->isHeaderToBeUpdated) {
			pSpriteElement->isHeaderToBeUpdated = 0;

			// Prepare header
			UBYTE isAttached = pSpriteElement->isAttached;
			UWORD uwVStart = s_pView->ubPosY + pSpriteElement->wY;
			UWORD uwVStop = uwVStart + pSpriteElement->uwHeight;
			UWORD uwHStart = s_pView->ubPosX - 1 + pSpriteElement->wX; // For diwstrt 0x81, x offset equal to 128 worked fine, hence -1

			// ptr to bitmap
			UWORD *pData = (UWORD *)pMultiplexedSprite->pBitmap->Planes[0];

			// Target the sprite header with offset
			tHardwareSpriteHeader *pHeader = (tHardwareSpriteHeader *)((void *)&pData[offset]); // 2 words per line
			//tHardwareSpriteHeader *pHeader = (tHardwareSpriteHeader*)(pMultiplexedSprite->pBitmap->Planes[0]);
			// Update header
			if (pSpriteElement->isEnabled) {
				pHeader->uwRawPos = ((uwVStart << 8) | ((uwHStart) >> 1));
				pHeader->uwRawCtl = (UWORD) (
					(uwVStop << 8) |
					(isAttached << 7) |
					(BTST(uwVStart, 8) << 2) |
					(BTST(uwVStop, 8) << 1) |
					BTST(uwHStart, 0)
				);
			}
			else {
				// if disabled so a blank line. (end of sprite date)
				pHeader->uwRawPos = 0;
				pHeader->uwRawCtl = 0;
			}
			offset += (pSpriteElement->uwHeight+1)*2; // next header ofset
		}
		// Update bitmap - if disabled we skip to save the blitcopy
		if (pSpriteElement->isBitmapToBeUpdated && pSpriteElement->isEnabled) {
			pSpriteElement->isBitmapToBeUpdated= 0;

			blitCopy(pSpriteElement->pBitmap,
                    0, 0,
                    pMultiplexedSprite->pBitmap,
                    0,offsetHeight, 
                    SPRITE_WIDTH, pSpriteElement->uwHeight,
                    MINTERM_COOKIE
                );
		}
		offsetHeight += pSpriteElement->uwHeight; // next line offset
	}
}

void spriteMultiplexedProcessChannel(UBYTE ubChannelIndex) {
	// For the moment same the sprite.c except it's a multiplexed sprite not sprite reusing the same field/interface

	tSpriteChannel *pChannel = &s_pChannelsData[ubChannelIndex];
	if(!pChannel->ubCopperRegenCount) {
		return;
	}

	// Update relevant part of current raw copperlist
	const tMultiplexedSprite *pSprite = pChannel->pFirstSprite;
	if(s_pView->pCopList->ubMode == COPPER_MODE_BLOCK && pChannel->pCopBlock) {
		pChannel->ubCopperRegenCount = 0;
		tCopBlock *pCopBlock = pChannel->pCopBlock;
		pCopBlock->uwCurrCount = 0;
		ULONG ulSprAddr = (
			pSprite->isEnabled ?
			(ULONG)(pSprite->pBitmap->Planes[0]) : (ULONG)s_pBlankSprite
		);
		copMove(
			s_pView->pCopList, pCopBlock,
			&g_pSprFetch[pSprite->ubChannelIndex].uwHi, ulSprAddr >> 16
		);
		copMove(
			s_pView->pCopList, pCopBlock,
			&g_pSprFetch[pSprite->ubChannelIndex].uwLo, ulSprAddr & 0xFFFF
		);
	}
	else {
		--pChannel->ubCopperRegenCount;
		UWORD uwRawCopPos = pChannel->uwRawCopPos;
		tCopCmd *pList = &s_pView->pCopList->pBackBfr->pList[uwRawCopPos];

		ULONG ulSprAddr = (
			pSprite && pSprite->isEnabled ?
			(ULONG)(pSprite->pBitmap->Planes[0]) : (ULONG)s_pBlankSprite
		);
		copSetMoveVal(&pList[0].sMove, ulSprAddr >> 16);
		copSetMoveVal(&pList[1].sMove, ulSprAddr & 0xFFFF);
	}
}

void spriteMultiplexedSetHeight(tMultiplexedSprite *pMultiplexedSprite,UBYTE spriteIndex, UWORD uwHeight) {
	
	if (pMultiplexedSprite->pMultiplexedSpriteElement[spriteIndex]->uwHeight != uwHeight) {
		pMultiplexedSprite->isBitmapToBeUpdated = 1;
		pMultiplexedSprite->pMultiplexedSpriteElement[spriteIndex]->uwHeight = uwHeight;
		pMultiplexedSprite->pMultiplexedSpriteElement[spriteIndex]->isHeaderToBeUpdated = 1;
		//TODO trigger a rebuild at the end of the function
	}

}

