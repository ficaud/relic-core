/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @file bip39_words.h
 *
 * @brief BIP-39 English wordlist (2048 words) declaration.
 *
 * @author Julien F.
 * @date 2026-08-25
 *
 * @details Exposes the official BIP-39 English wordlist as a const array of
 *          string pointers. The words are sorted, which allows a binary
 *          search (word -> index). The array and the strings are all const,
 *          so the linker places them in the .rodata section of the ESP32
 *          internal flash (memory-mapped, zero DRAM usage).
 *
 * @license
 *          This header (and its implementation) is released under
 *          GPL-3.0-or-later. The wordlist data defined in bip39_words.c
 *          originates from the bitcoin/bips repository (bip-0039/english.txt)
 *          and retains its original MIT terms; see bip39_words.c for details.
 */

#ifndef BIP39_WORDS_H
#define BIP39_WORDS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// ===========================================================================
// Definitions
// ===========================================================================
/* Number of words in the BIP-39 English wordlist (2^11). */
#define BIP39_WORD_COUNT 2048

// ===========================================================================
// Variables
// ===========================================================================
/* Sorted BIP-39 English words, indexed 0..2047. */
extern const char *const bip39_words[BIP39_WORD_COUNT];

#ifdef __cplusplus
}
#endif

#endif /* BIP39_WORDS_H */
