/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACE_MANAGERS_MULTIPLEXED_SPRITE_H_
#define _ACE_MANAGERS_MULTIPLEXED_SPRITE_H_

/**
 * @file multiplexedsprite.h
 * @brief The multiplexed sprite manager. "Reusing Sprite DMA Channels  " - http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node00C4.html
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <ace/utils/bitmap.h>
#include <ace/utils/extview.h>


typedef struct tMultiplexedSpriteElement {
	tBitMap *pBitmap;
	WORD wX; ///< X position, measured from the left of the view.
	WORD wY; ///< Y position, measured from the top of the view.
	UWORD uwHeight;
	UBYTE isEnabled;
	UBYTE isAttached; // Odd Sprites Only.
	UBYTE isHeaderToBeUpdated;
	UBYTE isBitmapToBeUpdated;
} tMultiplexedSpriteElement;

typedef struct tMultiplexedSprite {
	tBitMap *pBitmap;
	UWORD uwTotalHeight;
	UBYTE isEnabled;
	UBYTE ubChannelIndex;
	UBYTE isHeaderToBeUpdated;
	UBYTE isBitmapToBeUpdated;
	UBYTE ubSpriteCount;
	tMultiplexedSpriteElement **pMultiplexedSpriteElement;
} tMultiplexedSprite;

/**
 * @brief Add a multiplexed sprite on selected hardware channel.
 *
 *
 * @note This function may temporarily re-enable OS.
 *
 * @param ubChannelIndex Index of the channel. 0 is the first channel.
 * @param uwSpriteTotalHeight Total Height of sum of sprites.
 * @param ubNumberOfMultiplexedSprites Number of sprites to multiplex
 * @return Newly created tMultiplexedSprite struct on success, 0 on failure.
 *
 * @see spriteMultiplexedRemove()
 * @see spriteMultiplexedSetElement()
 */
tMultiplexedSprite *spriteMultiplexedAdd(UBYTE ubChannelIndex, UWORD uwSpriteTotalHeight,UBYTE ubNumberOfMultiplexedSprites);

/**
 * @brief Changes bitmap image used to display the one the multiplexed sprite.
 * NOT resizes sprite to bitmap height is allowed
 *
 * @note The sprite will write to the bitmap's bitplanes to update its control
 * words. If you need to use same bitmap for different sprites, be sure to have
 * separate copies.
 *
 * @param pMultiplexedSprite Sprite of which bitmap is to be updated.
 * @param ubSpriteIndex Index of the sprite in the multiplexed sprite.
 * @param uwSpriteHeight Height of the sprite in the multiplexed sprite.
 * @param isEnabled Set to 1 to enable sprite, otherwise set to 0.
 * @param isAttached Set to 1 to enable sprite attachment, otherwise set to 0.
 */
void spriteMultiplexedSetElement(tMultiplexedSprite *pMultiplexedSprite, UBYTE ubSpriteIndex, UWORD uwSpriteHeight, UBYTE isEnabled, UBYTE isAttached);

/**
 * @brief Removes given sprite from the display and destroys its struct.
 *
 * @note This function may temporarily re-enable OS.
 *
 * @param pMultiplexedSprite Sprite to be destroyed.
 * @see spriteAdd()
 */
void spriteMultiplexedRemove(tMultiplexedSprite *pMultiplexedSprite);

/**
 * @brief Changes bitmap image used to display the one the multiplexed sprite.
 * NOT resizes sprite to bitmap height is allowed
 *
 * @note The sprite will write to the bitmap's bitplanes to update its control
 * words. If you need to use same bitmap for different sprites, be sure to have
 * separate copies.
 *
 * @param pMultiplexedSprite Sprite of which bitmap is to be updated.
 * @param ubSpriteIndex Index of the sprite in the multiplexed sprite.
 * @param pBitmap Bitmap to be used for display/control data. The bitmap must be
 * in 2BPP interleaved format as well WITHOUT as start and end with an empty line,
 * which will not be displayed but used for storing control data.
 */
void spriteMultiplexedSetBitmap(tMultiplexedSprite *pMultiplexedSprite, UBYTE ubSpriteIndex, tBitMap *pBitmap);

/**
 * @brief Sets position of the sprite in the multiplexed sprite.
 *
 * @param pMultiplexedSprite Sprite of which position is to be set.
 * @param ubSpriteIndex Index of the sprite in the multiplexed sprite.
 * @param wX X position, measured from the left of the view.
 * @param wY Y position, measured from the top of the view.
 */
void spriteMultiplexedSpriteSetPos(tMultiplexedSprite *pMultiplexedSprite, UBYTE ubSpriteIndex, WORD wX, WORD wY);

/**
 * @brief Enables or disables a given sprite.
 *
 * @param pMultiplexedSprite Sprite to be enabled.
 * @param ubSpriteIndex Index of the sprite in the multiplexed sprite.
 * @param isEnabled Set to 1 to enable sprite, otherwise set to 0.
 */
void spriteMultiplexedSetEnabled(tMultiplexedSprite *pMultiplexedSprite, UBYTE ubSpriteIndex, UBYTE isEnabled);

/**
 * @brief Sets whether the sprite is an attached sprite.
 * Attached sprites are only available on odd sprite channels.
 *
 * @param isAttached Set to 1 to enable sprite attachment, otherwise set to 0.
 *
 * @see spriteProcess()
 */
void spriteMultiplexedSetAttached(tMultiplexedSprite *pMultiplexedSprite,  UBYTE spriteIndex, UBYTE isAttached);


/**
 * @brief Overrides sprite height to given value.
 * Also sets metadata update pending flag.
 *
 * @param pMultiplexedSprite Sprite of which height is to be changed.
 * @param spriteIndex Index of the sprite in the multiplexed sprite.
 * @param uwHeight New sprite height. Maximum is 511.
 */
void spriteMultiplexedSetHeight(tMultiplexedSprite *pMultiplexedSprite,UBYTE spriteIndex, UWORD uwHeight);

/**
 * @brief Sets metadata update as pending. Be sure to call it after
 * changing sprite's position, sizing or pointer to the next sprite.
 *
 * Multiple operations on sprite may independently require rebuilding
 * its metadata.
 * This function marks it as invalid so that it will be rebuilt only once
 * by spriteProcess() later on.
 *
 * @param pMultiplexedSprite Sprite of which metadata should be set to pending update.
 * @param spriteIndex Index of the sprite in the multiplexed sprite.
 *
 * @see spriteProcess()
 */
void spriteMultiplexedRequestMetadataUpdate(tMultiplexedSprite *pMultiplexedSprite, UBYTE spriteIndex);

/**
 * @brief Updates the sprite's metadata if set as requiring update.
 * Be sure to call it at least whenever sprite's metadata needs updating.
 *
 * @param pMultiplexedSprite Sprite of which screen position is to be updated.
 *
 * @see spriteRequestMetadataUpdate()
 * @see spriteProcessChannel()
 */
void spriteMultiplexedProcess(tMultiplexedSprite *pMultiplexedSprite);

/**
 * @brief Updates the given sprite channel.
 * Be sure to call it whenever the first sprite in channel have changed
 * and/or was enabled/disabled.
 *
 * @param ubChannelIndex
 *
 * @see spriteProcess()
 */
void spriteMultiplexedProcessChannel(UBYTE ubChannelIndex);

#ifdef __cplusplus
}
#endif

#endif // _ACE_MANAGERS_MULTIPLEXED_SPRITE_H_
