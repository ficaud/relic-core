/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @file slip39_words.h
 *
 * @brief SLIP-39 English wordlist (1024 words) declaration.
 *
 * @author Julien F.
 * @date 2026-09-15
 *
 * @details Exposes the official SLIP-39 (SLIP-0039) English wordlist as a
 *          const array of string pointers. The words are sorted, which allows
 *          a binary search (word -> index). The array and the strings are all
 *          const, so the linker places them in the .rodata section of the
 *          ESP32 internal flash (memory-mapped, zero DRAM usage).
 *
 * @license
 *          This header (and its implementation) is released under
 *          GPL-3.0-or-later. The wordlist data defined in slip39_words.c
 *          originates from the satoshilabs/slips repository
 *          (slip-0039/wordlist.txt) and retains its original MIT terms; see
 *          slip39_words.c for details.
 */

#ifndef SLIP39_WORDS_H
#define SLIP39_WORDS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// ===========================================================================
// Definitions
// ===========================================================================
/* Number of words in the SLIP-39 English wordlist (2^10). */
#define SLIP39_WORD_COUNT 1024

// ===========================================================================
// Variables
// ===========================================================================
/* Sorted SLIP-39 English words, indexed 0..1023. */
extern const char *const slip39_words[SLIP39_WORD_COUNT];

#ifdef __cplusplus
}
#endif

#endif /* SLIP39_WORDS_H */
