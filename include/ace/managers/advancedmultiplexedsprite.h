/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACE_MANAGERS_ADVANCED_MULTIPLEXED_SPRITE_H_
#define _ACE_MANAGERS_ADVANCED_MULTIPLEXED_SPRITE_H_

/**
 * @file advancedmultiplexedsprite.h
 * @brief Animated 16/32px, 2/4bpp multiplexed sprites on consecutive channels.
 *
 * Initialize spriteMultiplexedManagerCreate first. Frame pixels are owned copies;
 * input strips may be released immediately after Add. Each logical element has
 * independent position, frame and enable state. Four-bitplane images use attached
 * pairs: low planes on even channels, high planes with ATTACH on odd channels.
 * A 32px image uses a second column at X + 16, not a second attachment offset.
 *
 * Call Process, ProcessChannel and copper processing every frame, in that order.
 * The ordinary manager's beam synchronization, overlap suppression, hardware
 * coordinate limits and teardown requirements also apply. Each 16px column is
 * omitted if its hardware X is unrepresentable; attached halves stay together.
 * Invalid indices/frames and NULL sprite arguments are no-ops. Use the setters;
 * internal buffers, child managers, counts and frame pointers are manager-owned.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <ace/managers/multiplexedsprite.h>

typedef struct tSubMultiplexedSprite {
	WORD wX;
	WORD wY;
	UWORD uwAnimFrame;
	UBYTE isEnabled;
} tSubMultiplexedSprite;

typedef struct tAdvancedMultiplexedSprite {
	tMultiplexedSprite **pMultiplexedSprites;
	UBYTE ubMultiplexedCount;
	UBYTE ubSpriteCount;
	UWORD uwAnimCount;
	tBitMap **pAnimBitmap;
	UWORD uwHeight;
	UBYTE ubByteWidth;
	UBYTE uwWidth;
	UBYTE ubChannelIndex;
	UBYTE isEnabled; ///< Whole-object enable, sampled by Process each frame.
	UBYTE isHeaderToBeUpdated;
	tSubMultiplexedSprite **pMultiplexedSpriteElements;
	UBYTE is4PP;
} tAdvancedMultiplexedSprite;

/**
 * @brief Allocate initialized, enabled logical elements, initially at (0,0), frame 0.
 * @param ubChannelIndex First channel. 4bpp requires an even starting channel.
 * All required channels must be free and fit in 0..7: 16px/2bpp uses 1,
 * 16px/4bpp or 32px/2bpp uses 2, 32px/4bpp uses 4.
 * @param pStrip1 Required packed interleaved strip, 16 or 32px, depth 2 or 4.
 * @param pStrip2 Optional second strip with matching width/depth; frames follow
 * strip1. Each strip must contain whole frames, with no control/terminator rows.
 * @param uwSpriteHeight Pixel height (1..255). Total frame count must fit UWORD.
 * @param ubNumberOfMultiplexedSprites Logical element count (1..255).
 * @return Owned object, or NULL on invalid input/channel conflict/allocation failure.
 * Reported allocation failures release partial allocations; ACE helpers retain
 * their own internal allocation/error handling. OS access may be enabled.
 */
tAdvancedMultiplexedSprite *advancedMultiplexedSpriteAdd(
	UBYTE ubChannelIndex, tBitMap *pStrip1, tBitMap *pStrip2,
	UBYTE uwSpriteHeight, UBYTE ubNumberOfMultiplexedSprites
);

/**
 * @brief Free every child channel, copied frame, element and pointer array.
 * Disable sprite DMA first; apply ordinary Remove's copper drain/unload contract.
 * Call before spriteMultiplexedManagerDestroy and viewDestroy. NULL is a no-op.
 */
void advancedMultiplexedSpriteRemove(tAdvancedMultiplexedSprite *pSprite);

/** @brief Select a zero-based frame independently per logical element. */
void advancedMultiplexedSpriteSetFrame(
	tAdvancedMultiplexedSprite *pSprite, UBYTE ubIndex, UWORD uwFrame
);

/** @brief Enable/disable all hardware pieces of one logical element. */
void advancedMultiplexedSpriteSetEnabled(
	tAdvancedMultiplexedSprite *pSprite, UBYTE ubIndex, UBYTE isEnabled
);

/** @brief Set signed, view-relative position of one logical element. */
void advancedMultiplexedSpriteSetPos(
	tAdvancedMultiplexedSprite *pSprite, UBYTE ubIndex, WORD wX, WORD wY
);

/** @brief Propagate logical state and prepare all child DMA streams every frame. */
void advancedMultiplexedSpriteProcess(tAdvancedMultiplexedSprite *pSprite);

/** @brief Publish all prepared child streams before the frame's copper swap. */
void advancedMultiplexedSpriteProcessChannel(tAdvancedMultiplexedSprite *pSprite);

#ifdef __cplusplus
}
#endif

#endif // _ACE_MANAGERS_ADVANCED_MULTIPLEXED_SPRITE_H_
