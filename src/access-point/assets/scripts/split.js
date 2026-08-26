/**
 * split.js — Shamir's Secret Sharing split flow.
 *
 * Dual-mode: uses WASM (client-side) when sss.js is available (GitHub Pages
 * demo), falls back to fetch(/divide) on the embedded device.
 */

(function () {
    'use strict';

    var Module = null;
    var useWasm = false;

    /* ── WASM bootstrap ── */
    (function () {
        var script = document.createElement('script');
        script.src = 'scripts/sss.js';               // relative to HTML page
        script.onload = function () {
            SSS().then(function (m) { Module = m; useWasm = true; });
        };
        // If sss.js 404s or times out we stay in fetch mode – no action needed.
        script.onerror = function () { /* fetch fallback */ };
        document.head.appendChild(script);
    })();

    /* ── QR WASM bootstrap ── */
    var qrModule = null;
    (function () {
        var script = document.createElement('script');
        script.src = 'scripts/qr.js';                // relative to HTML page
        script.onload = function () {
            QRWasm().then(function (m) { qrModule = m; });
        };
        script.onerror = function () { /* fetch fallback */ };
        document.head.appendChild(script);
    })();

    /* ── Compression codec WASM bootstrap (share base32 + BIP-39) ── */
    var codecModule = null;
    (function () {
        var script = document.createElement('script');
        script.src = 'scripts/compression.js';       // relative to HTML page
        script.onload = function () {
            CompressionWasm().then(function (m) { codecModule = m; });
        };
        script.onerror = function () { /* fetch fallback */ };
        document.head.appendChild(script);
    })();

    /* ── Shared helpers ── */
    var msgInput = document.getElementById('msg-input');
    var msgBtn = document.getElementById('msg-btn');
    var shareList = document.getElementById('share-list');
    var toast = document.getElementById('toast');

    function showToast(text) {
        toast.textContent = text;
        toast.classList.add('visible');
        setTimeout(function () { toast.classList.remove('visible'); }, 1500);
    }

    function copyText(text, btn) {
        if (navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(text).then(function () {
                btn.classList.add('copied');
                setTimeout(function () { btn.classList.remove('copied'); }, 1000);
                showToast('Copied!');
            });
        } else {
            var ta = document.createElement('textarea');
            ta.value = text;
            ta.style.position = 'fixed';
            ta.style.opacity = '0';
            document.body.appendChild(ta);
            ta.select();
            document.execCommand('copy');
            document.body.removeChild(ta);
            btn.classList.add('copied');
            setTimeout(function () { btn.classList.remove('copied'); }, 1000);
            showToast('Copied!');
        }
    }

    function downloadQR(shareText, index) {
        /* Helper: trigger download from SVG text */
        function saveSvg(svgText) {
            var blob = new Blob([svgText], { type: 'image/svg+xml' });
            var a = document.createElement('a');
            a.href = URL.createObjectURL(blob);
            a.download = 'share-' + (index + 1) + '-qr.svg';
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(a.href);
            showToast('QR downloaded!');
        }

        /* Try WASM QR generator first (client-side, offline-capable) */
        if (qrModule && qrModule._wasm_qr_generate && codecModule && codecModule._wasm_share_to_base32) {
            try {
                /* Compress the hex share to base32 (same codec as the device)
                   to shrink the QR code. */
                var b32Ptr = codecModule.ccall('wasm_share_to_base32', 'number', ['string'], [shareText]);
                if (b32Ptr) {
                    var b32Text = codecModule.UTF8ToString(b32Ptr);
                    var svgPtr = qrModule.ccall('wasm_qr_generate', 'number', ['string'], [b32Text]);
                    codecModule._free(b32Ptr);
                    if (svgPtr) {
                        var svg = qrModule.UTF8ToString(svgPtr);
                        qrModule._wasm_qr_free(svgPtr);
                        saveSvg(svg);
                        return;
                    }
                }
            } catch (e) { /* fall through to fetch */ }
        }

        /* Fallback: server-side QR generation (embedded device).
           /qr-share.svg compresses the hex payload to base32 in C to shrink
           the QR code. */
        var url = '/qr-share.svg?text=' + encodeURIComponent(shareText);
        fetch(url)
            .then(function (r) {
                if (!r.ok) throw new Error('QR generation failed');
                return r.text();
            })
            .then(function (svg) { saveSvg(svg); })
            .catch(function (err) {
                showToast('Error: ' + err.message);
            });
    }

    function displayShares(shares) {
        shareList.innerHTML = '';
        shareList.classList.remove('hidden');

        var allText = '';

        shares.forEach(function (s, i) {
            var item = document.createElement('div');
            item.className = 'share-item';

            var label = document.createElement('span');
            label.className = 'share-label';
            label.textContent = '#' + (i + 1);
            item.appendChild(label);

            var data = document.createElement('span');
            data.className = 'share-data';
            data.textContent = s.d;
            item.appendChild(data);

            var btn = document.createElement('button');
            btn.className = 'copy-btn';
            btn.textContent = '\u{1F4CB}';
            btn.title = 'Copy share #' + (i + 1);
            item.appendChild(btn);

            var shareText = s.x + ':' + s.d;
            btn.addEventListener('click', function () {
                copyText(shareText, btn);
            });

            /* QR Code download button */
            var qrBtn = document.createElement('button');
            qrBtn.className = 'qr-btn';
            qrBtn.textContent = '\u{1F4F7}';
            qrBtn.title = 'Download QR code for share #' + (i + 1);
            qrBtn.addEventListener('click', function () {
                downloadQR(shareText, i);
            });
            item.appendChild(qrBtn);

            shareList.appendChild(item);

            if (i > 0) allText += '\n';
            allText += shareText;
        });

        var copyAllBtn = document.createElement('button');
        copyAllBtn.className = 'copy-all';
        copyAllBtn.textContent = '\u{1F4CB} Copy all shares';
        copyAllBtn.addEventListener('click', function () {
            copyText(allText, copyAllBtn);
        });
        shareList.appendChild(copyAllBtn);
    }

    /* ── WASM implementation ── */
    function sizeofShare() { return 264; }

    function makeShare(ptr) {
        var x = Module.getValue(ptr, 'i8') & 0xFF;
        var dataPtr = ptr + 1;
        var len = Module.getValue(ptr + 260, 'i32');
        var data = '';
        for (var i = 0; i < len; i++) {
            var b = Module.getValue(dataPtr + i, 'i8') & 0xFF;
            data += ('0' + b.toString(16)).slice(-2).toUpperCase();
        }
        return { x: x, len: len, d: data };
    }

    function encryptWasm(bytes) {
        var secretLen = bytes.length;
        var n = 5, k = 3;

        var seed = 0;
        if (window.crypto && window.crypto.getRandomValues) {
            var a = new Uint32Array(1);
            window.crypto.getRandomValues(a);
            seed = a[0];
        } else {
            seed = Date.now() ^ Math.random() * 0xFFFFFFFF;
        }
        Module._wasm_set_seed(seed);

        var secretPtr = Module._malloc(secretLen);
        var sharesPtr = Module._malloc(n * sizeofShare());

        for (var i = 0; i < secretLen; i++) {
            Module.setValue(secretPtr + i, bytes[i], 'i8');
        }

        var ret = Module._sss_split_wasm(secretPtr, secretLen, n, k, sharesPtr);
        if (ret !== 0) { showToast('Split failed'); return; }

        var shares = [];
        for (var si = 0; si < n; si++) {
            shares.push(makeShare(sharesPtr + si * sizeofShare()));
        }

        Module._free(secretPtr);
        Module._free(sharesPtr);

        displayShares(shares);
    }

    /* ── Fetch implementation (device) ── */
    function encryptFetch(msg, bip) {
        var url = '/divide?msg=' + encodeURIComponent(msg);
        if (bip) url += '&bip=' + bip;
        fetch(url)
            .then(function (r) {
                if (!r.ok) throw new Error('HTTP ' + r.status);
                return r.json();
            })
            .then(function (data) { displayShares(data); })
            .catch(function (err) { showToast('Error: ' + err.message); });
    }

    /* ── BIP-39 compression helpers (WASM demo) ── */
    function stringToBytes(s) {
        var bytes = [];
        for (var i = 0; i < s.length; i++) {
            bytes.push(s.charCodeAt(i) & 0xFF);
        }
        return bytes;
    }

    function hexToBytes(hex) {
        var bytes = [];
        for (var i = 0; i < hex.length; i += 2) {
            bytes.push(parseInt(hex.substr(i, 2), 16));
        }
        return bytes;
    }

    function bip39CompressWasm(msg) {
        if (!codecModule || !codecModule._wasm_bip39_compress) return null;
        var ptr = codecModule.ccall('wasm_bip39_compress', 'number', ['string'], [msg]);
        if (!ptr) return null;
        var hex = codecModule.UTF8ToString(ptr);
        codecModule._free(ptr);
        return hex;
    }

    function bip39CompressPassphraseWasm(msg) {
        if (!codecModule || !codecModule._wasm_bip39_compress_passphrase) return null;
        var ptr = codecModule.ccall('wasm_bip39_compress_passphrase', 'number', ['string'], [msg]);
        if (!ptr) return null;
        var hex = codecModule.UTF8ToString(ptr);
        codecModule._free(ptr);
        return hex;
    }

    /* ── Entry point ── */
    function bipMode() {
        var compression = document.getElementById('bip-compression').checked;
        var passphrase = document.getElementById('bip-passphrase').checked;
        if (!compression) return 0;
        return passphrase ? 2 : 1;
    }

    function encrypt() {
        var msg = msgInput.value;
        if (msg.trim() === '') return;
        var mode = bipMode();

        if (useWasm && Module && Module._sss_split_wasm) {
            var bytes;
            if (mode === 1) {
                var hex = bip39CompressWasm(msg);
                if (!hex) { showToast('BIP-39 compression failed'); return; }
                bytes = hexToBytes(hex);
            } else if (mode === 2) {
                var hexPass = bip39CompressPassphraseWasm(msg);
                if (!hexPass) { showToast('BIP-39 compression failed'); return; }
                bytes = hexToBytes(hexPass);
            } else {
                bytes = stringToBytes(msg);
            }
            encryptWasm(bytes);
        } else {
            encryptFetch(msg, mode);
        }
    }

    var bipCompressionCheckbox = document.getElementById('bip-compression');
    var bipPassphraseCheckbox = document.getElementById('bip-passphrase');
    bipCompressionCheckbox.addEventListener('change', function () {
        bipPassphraseCheckbox.disabled = !bipCompressionCheckbox.checked;
        if (!bipCompressionCheckbox.checked) bipPassphraseCheckbox.checked = false;
    });

    msgBtn.addEventListener('click', encrypt);
    msgInput.addEventListener('keydown', function (e) {
        if (e.key === 'Enter') encrypt();
    });
})();
