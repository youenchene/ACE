/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ace/managers/advancedmultiplexedsprite.h>
#include <ace/utils/bitmap.h>
#include <ace/managers/blit.h>
#include <ace/utils/sprite.h>
#include <ace/managers/system.h>


#define SPRITE_WIDTH 16
#define TWOBPP_BYTEWIDTH 2

tAdvancedMultiplexedSprite *advancedMultiplexedSpriteAdd(UBYTE ubChannelIndex, tBitMap *pSpriteVerticalStripBitmap1, tBitMap *pSpriteVerticalStripBitmap2, UBYTE uwSpriteHeight, UBYTE ubNumberOfMultiplexedSprites ) {
    tAdvancedMultiplexedSprite *pAdvancedMultiplexedSprite = memAllocFastClear(sizeof(*pAdvancedMultiplexedSprite));
    pAdvancedMultiplexedSprite->ubChannelIndex = ubChannelIndex;
    pAdvancedMultiplexedSprite->isEnabled = 1;
    pAdvancedMultiplexedSprite->uwHeight = uwSpriteHeight;
    pAdvancedMultiplexedSprite->pMultiplexedSpriteElements = (tSubMultiplexedSprite **)memAllocFastClear(sizeof(tSubMultiplexedSprite*) * ubNumberOfMultiplexedSprites);

    UBYTE ubDeltaHeaderHeight = 1; // 2 words for sprite control data
    
    // Check bitmap validity
    if(!(pSpriteVerticalStripBitmap1->Flags & BMF_INTERLEAVED) || (pSpriteVerticalStripBitmap1->Depth != 2  && (pSpriteVerticalStripBitmap1->Depth != 4 || (pAdvancedMultiplexedSprite->ubChannelIndex & 1) == 1))) {
        logWrite(
            "ERR: Sprite channel %hhu bitmap %p isn't interleaved 2BPP(for any channel) or 4BPP(for an even channel)\n",
            pAdvancedMultiplexedSprite->ubChannelIndex , pSpriteVerticalStripBitmap1
        );
        //return ;
    }
    pAdvancedMultiplexedSprite->ubByteWidth = bitmapGetByteWidth(pSpriteVerticalStripBitmap1);
    if(pAdvancedMultiplexedSprite->ubByteWidth != 2 && pAdvancedMultiplexedSprite->ubByteWidth != 4) {
        logWrite(
            "ERR: Unsupported sprite width: %hhu, expected 16 or 32\n",
            pAdvancedMultiplexedSprite->ubByteWidth * 8
        );
        //return;
    }

    // Calculate number of animation frames
    UWORD bStripe1NbAnim = pSpriteVerticalStripBitmap1->Rows / uwSpriteHeight;
    UWORD bStripe2NbAnim = 0;
    if (pSpriteVerticalStripBitmap2 != NULL) {
        bStripe2NbAnim = pSpriteVerticalStripBitmap2->Rows / uwSpriteHeight;
    }
    pAdvancedMultiplexedSprite->uwAnimCount = bStripe1NbAnim + bStripe2NbAnim;

    // Check if 4bpp
    if(pSpriteVerticalStripBitmap1->Depth == 4) {
        pAdvancedMultiplexedSprite->is4PP = 1;
    } else {
        pAdvancedMultiplexedSprite->is4PP = 0;
    }

    // Calculate number of sprites
    // 16px width
    if(pAdvancedMultiplexedSprite->ubByteWidth == 2) {
        if(pSpriteVerticalStripBitmap1->Depth == 2) {
            pAdvancedMultiplexedSprite->ubSpriteCount=1;
        } else if (pSpriteVerticalStripBitmap1->Depth == 4) {
            pAdvancedMultiplexedSprite->ubSpriteCount=2;
        }
    }
    // 32px width
    if(pAdvancedMultiplexedSprite->ubByteWidth == 4) {
        if(pSpriteVerticalStripBitmap1->Depth == 2) {
            pAdvancedMultiplexedSprite->ubSpriteCount=2;
        } else if (pSpriteVerticalStripBitmap1->Depth == 4) {
            pAdvancedMultiplexedSprite->ubSpriteCount=4;
        }
    }

    // Number of bitmaps to create
    UWORD nbBitmap=pAdvancedMultiplexedSprite->uwAnimCount*pAdvancedMultiplexedSprite->ubSpriteCount;
    // Limit for first bitmap
    UWORD nbBitmapLimit1=bStripe1NbAnim*pAdvancedMultiplexedSprite->ubSpriteCount;

    // Alloc memory for all frames (bitmap1 & bitmap2)
    pAdvancedMultiplexedSprite->pAnimBitmap= (tBitMap **)memAllocFastClear(nbBitmap*sizeof(tBitMap));

    // Temp bitmap for 4bpp to 2bpp conversion
    tBitMap *tmpBitmap = bitmapCreate(
        SPRITE_WIDTH, pAdvancedMultiplexedSprite->uwHeight,
        4, BMF_CLEAR | BMF_INTERLEAVED
    );
    tBitMap *pointers_low;
    tBitMap *pointers_high;

    pointers_high = bitmapCreate(
        TWOBPP_BYTEWIDTH*8, pAdvancedMultiplexedSprite->uwHeight,
        2, BMF_CLEAR | BMF_INTERLEAVED);

    pointers_low = bitmapCreate(
        TWOBPP_BYTEWIDTH*8, pAdvancedMultiplexedSprite->uwHeight,
        2, BMF_CLEAR | BMF_INTERLEAVED);

    tBitMap *pSpriteVerticalStripBitmap;
    pSpriteVerticalStripBitmap = pSpriteVerticalStripBitmap1;    
    UWORD k = 0;
    for (UWORD i = 0; i < nbBitmap;) {
        if (i == nbBitmapLimit1) {
            k=0;
            pSpriteVerticalStripBitmap = pSpriteVerticalStripBitmap2;
        }
        // One time if 16 pixel wide, Two times if 32 pixel wide
        for(unsigned j = 0; j < pAdvancedMultiplexedSprite->ubByteWidth/2; j++) {
            if (pAdvancedMultiplexedSprite->is4PP) {
                // Convert the 4bpp bitmap to 2bpp.
                blitCopy(
                    pSpriteVerticalStripBitmap, 0+j*SPRITE_WIDTH, k * pAdvancedMultiplexedSprite->uwHeight,
                    tmpBitmap,
                    0,0, // first line will be for sprite control data
                    SPRITE_WIDTH, pAdvancedMultiplexedSprite->uwHeight,
                    MINTERM_COOKIE
                );
                for (UWORD r = 0; r < tmpBitmap->Rows; r++) {
                    UWORD offetSrc = r * tmpBitmap->BytesPerRow;
                    UWORD offetDst = r * pointers_low->BytesPerRow;
                    memcpy(pointers_low->Planes[0] + offetDst, tmpBitmap->Planes[0] + offetSrc, TWOBPP_BYTEWIDTH);
                    memcpy(pointers_low->Planes[1] + offetDst, tmpBitmap->Planes[1] + offetSrc, TWOBPP_BYTEWIDTH);
                    memcpy(pointers_high->Planes[0] + offetDst, tmpBitmap->Planes[2] + offetSrc, TWOBPP_BYTEWIDTH);
                    memcpy(pointers_high->Planes[1] + offetDst, tmpBitmap->Planes[3] + offetSrc, TWOBPP_BYTEWIDTH);
                }
                // Init+2 on height for sprite management data - TODO: see if remove the last line is possible
                pAdvancedMultiplexedSprite->pAnimBitmap[i] = bitmapCreate(
                    SPRITE_WIDTH, pAdvancedMultiplexedSprite->uwHeight+ubDeltaHeaderHeight,
                    2, BMF_CLEAR | BMF_INTERLEAVED
                );
                blitCopy(
                    pointers_low, 0, 0,
                    pAdvancedMultiplexedSprite->pAnimBitmap[i],
                    0,1, // first line will be for sprite control data
                    SPRITE_WIDTH, pAdvancedMultiplexedSprite->uwHeight,
                    MINTERM_COOKIE
                );
                i++;
                // Init +delta on height for sprite management data
                pAdvancedMultiplexedSprite->pAnimBitmap[i] = bitmapCreate(
                    SPRITE_WIDTH, pAdvancedMultiplexedSprite->uwHeight+ubDeltaHeaderHeight,
                    2, BMF_CLEAR | BMF_INTERLEAVED
                );
                blitCopy(
                    pointers_high, 0, 0,
                    pAdvancedMultiplexedSprite->pAnimBitmap[i],
                    0,1, // first line will be for sprite control data
                    SPRITE_WIDTH, pAdvancedMultiplexedSprite->uwHeight,
                    MINTERM_COOKIE
                );
                i++;
            } else {
                // Init +2 on height for sprite management data  - TODO: see if remove the last line is possible
                pAdvancedMultiplexedSprite->pAnimBitmap[i] = bitmapCreate(
                    SPRITE_WIDTH, pAdvancedMultiplexedSprite->uwHeight+ubDeltaHeaderHeight,
                    pSpriteVerticalStripBitmap->Depth, BMF_CLEAR | BMF_INTERLEAVED
                );
                // Copy bitmap
                blitCopy(
                    pSpriteVerticalStripBitmap, 0+j*SPRITE_WIDTH, k * pAdvancedMultiplexedSprite->uwHeight,
                    pAdvancedMultiplexedSprite->pAnimBitmap[i],
                    0,1, // first line will be for sprite control data
                    SPRITE_WIDTH, pAdvancedMultiplexedSprite->uwHeight,
                    MINTERM_COOKIE
                ); 
                i++;
            }
        }
        k++;
    }
    bitmapDestroy(pointers_low);
    bitmapDestroy(pointers_high);
    bitmapDestroy(tmpBitmap);

    pAdvancedMultiplexedSprite->pMultiplexedSprites = (tMultiplexedSprite **)memAllocFastClear(sizeof(tMultiplexedSprite*) * pAdvancedMultiplexedSprite->ubSpriteCount);

    for (UWORD i = 0; i < pAdvancedMultiplexedSprite->ubSpriteCount; i++) {
        if (pAdvancedMultiplexedSprite->is4PP) {
            // 2 channels for 4bpp sprites
            pAdvancedMultiplexedSprite->pMultiplexedSprites[i] = spriteMultiplexedAdd(ubChannelIndex+i, uwSpriteHeight, ubNumberOfMultiplexedSprites);
            for (UBYTE indexMultiplexed = 0; indexMultiplexed < ubNumberOfMultiplexedSprites; indexMultiplexed++) {
                spriteMultiplexedSetElement(pAdvancedMultiplexedSprite->pMultiplexedSprites[i], indexMultiplexed, uwSpriteHeight, 1, 0);
                spriteMultiplexedSetBitmap(pAdvancedMultiplexedSprite->pMultiplexedSprites[i], indexMultiplexed, pAdvancedMultiplexedSprite->pAnimBitmap[i]);
            }
            i++;
            //attached sprite
            pAdvancedMultiplexedSprite->pMultiplexedSprites[i] = spriteMultiplexedAdd(ubChannelIndex+i, uwSpriteHeight,ubNumberOfMultiplexedSprites);
            for (UBYTE indexMultiplexed = 0; indexMultiplexed < ubNumberOfMultiplexedSprites; indexMultiplexed++) {
                spriteMultiplexedSetElement(pAdvancedMultiplexedSprite->pMultiplexedSprites[i], indexMultiplexed, uwSpriteHeight, 1, 1);
                spriteMultiplexedSetBitmap(pAdvancedMultiplexedSprite->pMultiplexedSprites[i], indexMultiplexed, pAdvancedMultiplexedSprite->pAnimBitmap[i]);
            } 
        } else {
            pAdvancedMultiplexedSprite->pMultiplexedSprites[i] = spriteMultiplexedAdd(ubChannelIndex+i, uwSpriteHeight,ubNumberOfMultiplexedSprites);
            for (UBYTE indexMultiplexed = 0; indexMultiplexed < ubNumberOfMultiplexedSprites; indexMultiplexed++) {
                spriteMultiplexedSetElement(pAdvancedMultiplexedSprite->pMultiplexedSprites[i], indexMultiplexed, uwSpriteHeight, 1, 0);
                spriteMultiplexedSetBitmap(pAdvancedMultiplexedSprite->pMultiplexedSprites[i], indexMultiplexed, pAdvancedMultiplexedSprite->pAnimBitmap[i]);
            }
        }      
    }

    return pAdvancedMultiplexedSprite;
}

void advancedMultiplexedSpriteRemove(tAdvancedMultiplexedSprite *pAdvancedMultiplexedSprite) {
    systemUse();
    for (UWORD i = 0; i < pAdvancedMultiplexedSprite->ubSpriteCount; i++) {
        spriteMultiplexedRemove(pAdvancedMultiplexedSprite->pMultiplexedSprites[i]);
    }
    for (UWORD i = 0; i < pAdvancedMultiplexedSprite->uwAnimCount; i++) {
        bitmapDestroy(pAdvancedMultiplexedSprite->pAnimBitmap[i]);
    }
    memFree(pAdvancedMultiplexedSprite, sizeof(*pAdvancedMultiplexedSprite));
    systemUnuse();
}

void advancedMultiplexedSpriteSetEnabled(tAdvancedMultiplexedSprite *pAdvancedMultiplexedSprite, UBYTE index, UBYTE isEnabled) {
    pAdvancedMultiplexedSprite->pMultiplexedSpriteElements[index]->isEnabled = isEnabled;
    pAdvancedMultiplexedSprite->isHeaderToBeUpdated = 1;
}

void advancedMultiplexedSpriteSetPos(tAdvancedMultiplexedSprite *pAdvancedMultiplexedSprite, UBYTE index,WORD wX, WORD wY) {
    pAdvancedMultiplexedSprite->pMultiplexedSpriteElements[index]->wX = wX;
    pAdvancedMultiplexedSprite->pMultiplexedSpriteElements[index]->wY = wY;
    pAdvancedMultiplexedSprite->isHeaderToBeUpdated = 1;
}

void advancedMultiplexedSpriteSetFrame(tAdvancedMultiplexedSprite *pAdvancedMultiplexedSprite, UBYTE index, UWORD animFrame) {
    if (animFrame >= pAdvancedMultiplexedSprite->uwAnimCount) {
        logWrite("ERR: Invalid animation index %hu\n", animFrame);
        return;
    }
    pAdvancedMultiplexedSprite->pMultiplexedSpriteElements[index]->uwAnimFrame=animFrame;
    UWORD animIndex = animFrame << ((pAdvancedMultiplexedSprite->ubByteWidth == 4) + pAdvancedMultiplexedSprite->is4PP);
    for (UWORD i = 0; i < pAdvancedMultiplexedSprite->ubSpriteCount; i++) {
        spriteMultiplexedSetBitmap(pAdvancedMultiplexedSprite->pMultiplexedSprites[i], index, pAdvancedMultiplexedSprite->pAnimBitmap[animIndex+i]);
    } 
    pAdvancedMultiplexedSprite->isHeaderToBeUpdated = 1;    
}

void advancedMultiplexedSpriteProcessChannel(tAdvancedMultiplexedSprite *pAdvancedMultiplexedSprite) {
    for (UWORD i = 0; i < pAdvancedMultiplexedSprite->ubSpriteCount; i++) {
        spriteMultiplexedProcessChannel(pAdvancedMultiplexedSprite->ubChannelIndex + i);
    }
}


UWORD addMultiplexAttachedX(tAdvancedMultiplexedSprite *pAdvancedMultiplexedSprite, UBYTE spriteindex) {
    if (( pAdvancedMultiplexedSprite->ubByteWidth==4) && (( (spriteindex>1) && (pAdvancedMultiplexedSprite->is4PP== 1) ) || ( (spriteindex==1) && (pAdvancedMultiplexedSprite->is4PP==0) )))
    {
        return SPRITE_WIDTH;
    }
    return 0;
}

void advancedMultiplexedSpriteProcess(tAdvancedMultiplexedSprite *pAdvancedMultiplexedSprite) {
    if(!pAdvancedMultiplexedSprite->isHeaderToBeUpdated) {
        return;
    }
    for (UBYTE indexAdvanced = 0; indexAdvanced < pAdvancedMultiplexedSprite->ubSpriteCount; indexAdvanced++) {
        for (UBYTE indexMultiplexed=0; indexMultiplexed < pAdvancedMultiplexedSprite->ubMultiplexedCount; indexMultiplexed++) {
            spriteMultiplexedSpriteSetPos(pAdvancedMultiplexedSprite->pMultiplexedSprites[indexAdvanced], indexMultiplexed ,pAdvancedMultiplexedSprite->pMultiplexedSpriteElements[indexAdvanced]->wX + addMultiplexAttachedX(pAdvancedMultiplexedSprite, indexAdvanced), pAdvancedMultiplexedSprite->pMultiplexedSpriteElements[indexAdvanced]->wY);
            spriteMultiplexedSetEnabled(pAdvancedMultiplexedSprite->pMultiplexedSprites[indexAdvanced], indexMultiplexed, pAdvancedMultiplexedSprite->pMultiplexedSpriteElements[indexAdvanced]->isEnabled);
        }
        spriteMultiplexedProcess(pAdvancedMultiplexedSprite->pMultiplexedSprites[indexAdvanced]);
    }
    pAdvancedMultiplexedSprite->isHeaderToBeUpdated=0;
}