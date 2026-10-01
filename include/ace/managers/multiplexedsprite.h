/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACE_MANAGERS_MULTIPLEXED_SPRITE_H_
#define _ACE_MANAGERS_MULTIPLEXED_SPRITE_H_

/**
 * @file multiplexedsprite.h
 * @brief DMA-chained, 16-pixel, 2bpp sprites on one hardware channel.
 *
 * Elements are scheduled by ascending Y, then logical index. Disabled elements
 * and unrepresentable hardware coordinates are omitted, not DMA terminators.
 * An element overlapping the last accepted element is omitted for that frame:
 * its VSTART must be at least the previous VSTOP + 1 (one DMA reload line).
 * Positions are view-relative; hardware HSTART must be 0..511, VSTART >= 26,
 * VSTOP <= 511. There is no vertical clipping or channel reassignment.
 *
 * Every displayed frame: set state, Process(), ProcessChannel(), then process
 * and swap the view's copper list exactly once. Do this for ALL managed channels,
 * even unchanged ones. Two DMA buffers are tied to the two copper buffers.
 * The caller must synchronize to the display: before writing the next back
 * buffer, the previous frame using it must have finished (e.g. the demo's
 * vPortWaitForEnd loop). Neither Process nor ProcessChannel waits for the beam.
 * Do not modify source pixels concurrently with Process.
 *
 * NULL sprite arguments and invalid indices are no-ops. Invalid setter values
 * leave state unchanged, except attachment on even channels is forced off.
 * Struct layout is exposed for compatibility, not binary ABI stability; use
 * setters rather than mutating internal buffers, counts or pointers.
 * Allocation failures reported by ACE helpers are propagated; this manager does
 * not replace those helpers' internal allocation/error handling.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <ace/utils/bitmap.h>
#include <ace/utils/extview.h>

typedef struct tMultiplexedSpriteElement {
	tBitMap *pBitmap; ///< Borrowed pixel-only source, never modified by the manager.
	WORD wX;
	WORD wY;
	UWORD uwHeight;
	UBYTE isEnabled;
	UBYTE isAttached;
	UBYTE isHeaderToBeUpdated;
	UBYTE isBitmapToBeUpdated; ///< Internal two-buffer dirty mask.
	UWORD pBufferOffset[2]; ///< Internal cached DMA row offsets.
} tMultiplexedSpriteElement;

typedef struct tMultiplexedSprite {
	tBitMap *pBitmap; ///< Most recently prepared DMA stream; manager-owned.
	UWORD uwTotalHeight;
	UBYTE isEnabled; ///< Whole-channel enable, sampled by Process each frame.
	UBYTE ubChannelIndex;
	UBYTE isHeaderToBeUpdated;
	UBYTE isBitmapToBeUpdated;
	UBYTE ubSpriteCount;
	tMultiplexedSpriteElement **pMultiplexedSpriteElement;
	UWORD uwMaxHeight;
	tBitMap *pDmaBitmap[2];
	tCopBfr *pBufferIdentity;
	tCopBfr *pPreparedBuffer;
	UBYTE *pOrder;
} tMultiplexedSprite;

/**
 * @brief Initialize once per manager lifetime, before Add and viewLoad.
 * @param pView Live view, retained until ManagerDestroy.
 * @param uwRawCopPos First of 16 reserved raw copper MOVEs (ignored in block mode).
 * @param pBlankSprite Optional borrowed, zeroed, chip-memory ULONG. Retain until
 * teardown; NULL requests an owned allocation. Do not share channels with another
 * sprite manager. Initialization/allocation failure makes Add return NULL.
 */
void spriteMultiplexedManagerCreate(
	const tView *pView, UWORD uwRawCopPos, ULONG pBlankSprite[1]
);

/**
 * @brief Free remaining ordinary sprites, owned blank data and copper blocks.
 * Remove advanced wrappers FIRST. Disable sprite DMA and unload the view before
 * calling; call BEFORE viewDestroy. NULL/uninitialized manager is a no-op.
 * OS access may temporarily be enabled. Recreate only after teardown.
 */
void spriteMultiplexedManagerDestroy(void);

/**
 * @brief Reserve one free channel (0..7) and initialize all elements disabled.
 * @param uwSpriteHeight Maximum pixel height per element (1..255).
 * @param ubNumberOfMultiplexedSprites Element capacity (1..255).
 * @return Owned sprite, or NULL for invalid arguments, occupied channel or
 * allocation failure. OS access may temporarily be enabled.
 */
tMultiplexedSprite *spriteMultiplexedAdd(
	UBYTE ubChannelIndex, UBYTE uwSpriteHeight,
	UBYTE ubNumberOfMultiplexedSprites
);

/**
 * @brief Configure an existing element, without allocating or discarding pixels.
 * Height must be 1..the constructor maximum and no greater than source Rows.
 * Attachment is supported only on odd channels; coordinate both channels of an
 * attached pair yourself, or use the advanced manager.
 */
void spriteMultiplexedSetElement(
	tMultiplexedSprite *pSprite, UBYTE ubIndex, UWORD uwHeight,
	UBYTE isEnabled, UBYTE isAttached
);

/**
 * @brief Release both DMA streams and elements; borrowed bitmaps are not freed.
 * Sprite DMA must be disabled before removal. Keep it disabled until both copper
 * buffers have been rebuilt with blank/replacement pointers (ProcessChannel and
 * copper processing twice for raw mode), or unload the view. Copper blocks are
 * retained for reuse until ManagerDestroy/viewDestroy. May temporarily enable OS.
 */
void spriteMultiplexedRemove(tMultiplexedSprite *pSprite);

/**
 * @brief Borrow a packed interleaved 16px, 2bpp pixel-only bitmap; set height to Rows.
 * No control/terminator rows in the source. Rows must be 1..constructor maximum.
 * Retain the source until replacement/removal, including while disabled.
 * The source is never written. Calling again with the same pointer requests a
 * refresh of edited pixels in both DMA buffers.
 */
void spriteMultiplexedSetBitmap(
	tMultiplexedSprite *pSprite, UBYTE ubIndex, tBitMap *pBitmap
);

/** @brief Set signed, view-relative coordinates; no unsigned coordinate wrapping. */
void spriteMultiplexedSpriteSetPos(
	tMultiplexedSprite *pSprite, UBYTE ubIndex, WORD wX, WORD wY
);

/** @brief Enable/disable an element without hiding later elements in the chain. */
void spriteMultiplexedSetEnabled(
	tMultiplexedSprite *pSprite, UBYTE ubIndex, UBYTE isEnabled
);

/** @brief Set ATTACH on an odd channel; even channels always clear ATTACH. */
void spriteMultiplexedSetAttached(
	tMultiplexedSprite *pSprite, UBYTE ubIndex, UBYTE isAttached
);

/** @brief Set height within 1..constructor maximum and available source Rows. */
void spriteMultiplexedSetHeight(
	tMultiplexedSprite *pSprite, UBYTE ubIndex, UWORD uwHeight
);

/**
 * @brief Invalidate ordering/pixels after legacy direct metadata edits.
 * Direct edits must still obey the setter invariants; prefer the setters.
 */
void spriteMultiplexedRequestMetadataUpdate(
	tMultiplexedSprite *pSprite, UBYTE ubIndex
);

/**
 * @brief Prepare the back-copper-associated DMA stream, including its terminator.
 * Call every frame before ProcessChannel, even when no setters were called.
 * No allocation, no logging, no asynchronous blits; waits for prior blits before
 * CPU reads. Unchanged pixel data at unchanged offsets is reused per buffer.
 */
void spriteMultiplexedProcess(tMultiplexedSprite *pSprite);

/**
 * @brief Publish this frame's prepared stream to the back copper list/block.
 * Call for every owned channel before each copper swap, including static frames.
 * Invalid channel numbers are no-ops. See the file-level synchronization contract.
 */
void spriteMultiplexedProcessChannel(UBYTE ubChannelIndex);

#ifdef __cplusplus
}
#endif

#endif // _ACE_MANAGERS_MULTIPLEXED_SPRITE_H_
