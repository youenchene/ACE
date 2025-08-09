/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACE_MANAGERS_ADVANCED_MULTIPLEXED_SPRITE_H_
#define _ACE_MANAGERS_ADVANCED_MULTIPLEXED_SPRITE_H_

/**
 * @file sprite.h
 * @brief The advanced for multiplex sprite manager which rely on the multiplexed sprite manager.
 *
 * @todo Multiplexed sprites
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <ace/utils/bitmap.h>
#include <ace/utils/extview.h>
#include <ace/managers/multiplexedsprite.h>


typedef struct tSubMultiplexedSprite {
    WORD wX; ///< X position, measured from the left of the view.
    WORD wY; ///< Y position, measured from the top of the view.
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
    UBYTE isEnabled;
    UBYTE isHeaderToBeUpdated;
    tSubMultiplexedSprite **pMultiplexedSpriteElements;
    UBYTE is4PP; // 16-color sprites only.
} tAdvancedMultiplexedSprite;

/**
 * @brief Add sprite on selected hardware channel.
 * Currently, only single sprite per channel is supported.
 *
 * @note This function may temporarily re-enable OS.
 *
 * @param ubChannelIndex Index of the channel. 0 is the first channel.
 * @param pSpriteVerticalStripBitmap1 Bitmap vertical strip to be used to display sprite and it's animation. Bitmap width is the target sprite width (16px or 32px).
 * @param pSpriteVerticalStripBitmap2 Bitmap vertical strip to be used to display sprite and it's animation. Bitmap width is the target sprite width (16px or 32px).
 * @param uwSpriteHeight Height of the sprite.
 * @param ubNumberOfMultiplexedSprites Number of multiplexed sprites to be set.
 * @return Newly created advanced sprite struct on success, 0 on failure.
 *
 * @see advancedSpriteRemove()
 */
tAdvancedMultiplexedSprite *advancedMultiplexedSpriteAdd(UBYTE ubChannelIndex, tBitMap *pSpriteVerticalStripBitmap1, tBitMap *pSpriteVerticalStripBitmap2, UBYTE uwSpriteHeight, UBYTE ubNumberOfMultiplexedSprites);

/**
 * @brief Removes given sprite from the display and destroys its struct.
 *
 * @note This function may temporarily re-enable OS.
 *
 * @param tAdvancedMultiplexedSprite Advanced Sprite to be destroyed.
 * @see advancedSpriteAdd()
 */
void advancedMultiplexedSpriteRemove(tAdvancedMultiplexedSprite *tAdvancedMultiplexedSprite);

/**
 * @brief Set Anim/content of sprite to display.
 *
 * @param pAdvancedMultiplexedSprite Sprite to be enabled.

 */
void advancedMultiplexedSpriteSetFrame(tAdvancedMultiplexedSprite *pAdvancedMultiplexedSprite,  UBYTE index, UWORD animFrame);

/**
 * @brief Enables or disables a given sprite.
 *
 * @param pAdvancedMultiplexedSprite Sprite to be enabled.
 * @param isEnabled Set to 1 to enable sprite, otherwise set to 0.
 */
void advancedMultiplexedSpriteSetEnabled(tAdvancedMultiplexedSprite *pAdvancedMultiplexedSprite,  UBYTE index, UBYTE isEnabled);

/**
 * @brief Set Position of the sprite.
 *
 * @param pAdvancedMultiplexedSprite Sprite to be enabled.
 * @param wX X position, measured from the left of the view.
 * @param wY Y position, measured from the top of the view.
 */
void advancedMultiplexedSpriteSetPos(tAdvancedMultiplexedSprite *pAdvancedMultiplexedSprite, UBYTE index, WORD wX, WORD wY);

/**
 * @brief Updates the sprite's metadata if set as requiring update.
 * Be sure to call it at least whenever sprite's metadata needs updating.
 *
 * @param pAdvancedMultiplexedSprite Sprite of which screen position is to be updated.
 *
 * @see advancedSpriteProcessChannel()
 */
void advancedMultiplexedSpriteProcess(tAdvancedMultiplexedSprite *pAdvancedMultiplexedSprite);

/**
 * @brief Updates the given sprite channel.
 * Be sure to call it whenever the first sprite in channel have changed
 * and/or was enabled/disabled.
 *
 * @param ubChannelIndex
 *
 * @see advancedSpriteProcess()
 */
void advancedMultiplexedSpriteProcessChannel(tAdvancedMultiplexedSprite *pAdvancedMultiplexedSprite);

#ifdef __cplusplus
}
#endif

#endif // _ACE_MANAGERS_ADVANCED_MULTIPLEXED_SPRITE_H_
