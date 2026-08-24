/**
 * unsplit.js — Shamir's Secret Sharing reconstruct flow.
 *
 * Dual-mode: uses WASM (client-side) when sss.js is available (GitHub Pages
 * demo), falls back to fetch(/reconstruct) on the embedded device.
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
        script.onerror = function () { /* fetch fallback */ };
        document.head.appendChild(script);
    })();

    /* ── QR decode WASM bootstrap (quirc) ──
       Only present in the WASM demo. On the embedded device qr_decode.js does
       not exist, so script.onerror fires and decoding is done on-device via
       POST /qr_decode. In the demo the WASM module is the local fallback. ── */
    var qrDecodeModule = null;

    (function () {
        var script = document.createElement('script');
        script.src = 'scripts/qr_decode.js';
        script.onload = function () {
            QRDecodeWasm().then(function (m) {
                qrDecodeModule = m;
                console.log('[relic-core] QR decoder: quirc (WASM) available as fallback');
            });
        };
        script.onerror = function () {
            console.log('[relic-core] QR decoder: on-device (POST /qr_decode)');
        };
        document.head.appendChild(script);
    })();

    /* ── Shared helpers ── */
    var shareRows = document.querySelectorAll('.share-row');
    var unsplitBtn = document.getElementById('unsplit-btn');
    var resultBox = document.getElementById('result-box');
    var resultText = document.getElementById('result-text');
    var copyResultBtn = document.getElementById('copy-result');
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

    /* ── Parse share rows ── */
    function parseShares() {
        var d = [], x = [];
        shareRows.forEach(function (row) {
            var xInput = row.querySelector('.x-input');
            var dInput = row.querySelector('.d-input');
            var val = dInput.value.trim();
            if (val === '') return;

            var colon = val.indexOf(':');
            if (colon !== -1) {
                d.push(val.substring(colon + 1));
                x.push(parseInt(val.substring(0, colon), 10));
            } else {
                d.push(val);
                x.push(parseInt(xInput.value.trim(), 10) || 0);
            }
        });
        return { d: d, x: x };
    }

    /* ── WASM implementation ── */
    function sizeofShare() { return 264; }

    function reconstructWasm(d, x) {
        var k = d.length;
        var secretLen = d[0].length / 2;

        var sharesPtr = Module._malloc(k * sizeofShare());
        var secretPtr = Module._malloc(secretLen);

        for (var si = 0; si < k; si++) {
            var sp = sharesPtr + si * sizeofShare();
            Module.setValue(sp, x[si], 'i8');                 // share.x
            for (var j = 0; j < secretLen; j++) {
                var byteVal = parseInt(d[si].substr(j * 2, 2), 16);
                Module.setValue(sp + 1 + j, byteVal, 'i8');   // share.data[j]
            }
            Module.setValue(sp + 260, secretLen, 'i32');      // share.len
        }

        var ret = Module._sss_combine_wasm(sharesPtr, k, secretPtr, secretLen);
        if (ret !== 0) { showToast('Reconstruction failed'); return; }

        var secret = '';
        for (var bi = 0; bi < secretLen; bi++) {
            secret += String.fromCharCode(Module.getValue(secretPtr + bi, 'i8') & 0xFF);
        }

        Module._free(sharesPtr);
        Module._free(secretPtr);

        resultText.textContent = secret || '(empty)';
        resultBox.classList.remove('hidden');
        showToast('Reconstructed!');
    }

    /* ── Fetch implementation (device) ── */
    function reconstructFetch(d, x) {
        var url = '/reconstruct?d=' + d.join(',') + '&x=' + x.join(',');
        fetch(url)
            .then(function (r) {
                if (!r.ok) throw new Error('HTTP ' + r.status);
                return r.json();
            })
            .then(function (data) {
                resultText.textContent = data.secret || '(empty)';
                resultBox.classList.remove('hidden');
            })
            .catch(function (err) { showToast('Error: ' + err.message); });
    }

    /* ── Entry point ── */
    function reconstruct() {
        var parsed = parseShares();
        if (parsed.d.length < 2) { showToast('Enter at least 2 shares'); return; }

        if (useWasm && Module && Module._sss_combine_wasm) {
            reconstructWasm(parsed.d, parsed.x);
        } else {
            reconstructFetch(parsed.d, parsed.x);
        }
    }

    unsplitBtn.addEventListener('click', reconstruct);

    shareRows.forEach(function (row) {
        var dInput = row.querySelector('.d-input');
        dInput.addEventListener('keydown', function (e) {
            if (e.key === 'Enter') reconstruct();
        });
    });

    copyResultBtn.addEventListener('click', function () {
        copyText(resultText.textContent, copyResultBtn);
    });

    /* ── QR Code scanner (dual-mode: live camera or file picker) ── */
    var qrFileInput = document.getElementById('qr-file-input');
    var qrOverlay    = document.getElementById('qr-overlay');
    var qrVideo      = document.getElementById('qr-video');
    var qrStatus     = document.getElementById('qr-status');
    var qrClose      = document.getElementById('qr-close');
    var qrCanvas     = document.createElement('canvas');
    var qrCtx        = qrCanvas.getContext('2d', { willReadFrequently: true });
    var targetRow    = null;
    var qrStream     = null;
    var qrAnim       = null;
    var cameraFailed = false;

    var qrBtns = document.querySelectorAll('.qr-btn');
    qrBtns.forEach(function (btn) {
        btn.addEventListener('click', function () {
            targetRow = parseInt(btn.getAttribute('data-row'), 10);
            openQRScanner();
        });
    });

    function openQRScanner() {
        // Only try live camera on secure contexts (HTTPS / localhost).
        // On HTTP (ESP32 on 192.168.4.1) or after a previous failure we go
        // directly to file picker — synchronous .click() works on iOS Safari.
        if (!cameraFailed && window.isSecureContext &&
            navigator.mediaDevices && navigator.mediaDevices.getUserMedia) {
            navigator.mediaDevices.getUserMedia({
                video: { facingMode: 'environment', width: { ideal: 640 }, height: { ideal: 480 } }
            })
                .then(function (stream) {
                    qrStream = stream;
                    qrVideo.srcObject = stream;
                    qrVideo.play();
                    qrOverlay.classList.remove('hidden');
                    qrStatus.textContent = 'Point the camera at a QR code';
                    startQRScan();
                })
                .catch(function () {
                    cameraFailed = true;
                    showToast('Camera unavailable — tap \uD83D\uDCF7 again');
                });
        } else {
            // Synchronous user-gesture path — no setTimeout!
            qrFileInput.click();
        }
    }

    /* ── QR decode ──
       The embedded firmware replaces the placeholders with per-board values:
       - __QR_MAX_DIM__: max grayscale dimension accepted by the device
         (224 on ESP32-S3, 192 on the classic ESP32). In the WASM demo the
         placeholder stays, so parseInt() yields NaN and we use 224.
       - __QR_DECODE_SERVER__: 1 when the ESP32 decodes (ESP32-S3), 0 when
         the page must decode locally (classic ESP32 / WASM demo).
       Local decoding uses WASM quirc (demo) or jsQR (classic ESP32). ── */
    var QR_CLIENT_MAX_DIM = parseInt('__QR_MAX_DIM__', 10) || 224;
    var QR_DECODE_SERVER  = parseInt('__QR_DECODE_SERVER__', 10) === 1;
    var QR_SCAN_INTERVAL_MS = 250;

    function drawToGray(src, naturalW, naturalH) {
        var scale = Math.min(1, QR_CLIENT_MAX_DIM / Math.max(naturalW, naturalH));
        var w = Math.max(1, Math.round(naturalW * scale));
        var h = Math.max(1, Math.round(naturalH * scale));

        qrCanvas.width  = w;
        qrCanvas.height = h;
        qrCtx.imageSmoothingEnabled = true;
        qrCtx.drawImage(src, 0, 0, w, h);

        var rgba = qrCtx.getImageData(0, 0, w, h).data;
        var gray = new Uint8Array(w * h);
        for (var i = 0; i < w * h; i++) {
            var r = rgba[i * 4], g = rgba[i * 4 + 1], b = rgba[i * 4 + 2];
            // ITU-R BT.601 luma — the standard grayscale conversion.
            gray[i] = (0.299 * r + 0.587 * g + 0.114 * b) | 0;
        }
        return { gray: gray, w: w, h: h };
    }

    /* ── Device decode (ESP32-S3): POST the grayscale frame to /qr_decode,
       where quirc runs on-device. ── */
    function decodeWithServer(gray, w, h) {
        if (!gray || !w || !h) return Promise.resolve(null);
        return fetch('/qr_decode?w=' + w + '&h=' + h, {
            method: 'POST',
            headers: { 'Content-Type': 'application/octet-stream' },
            body: gray
        })
            .then(function (r) {
                if (!r.ok) return null;
                return r.json().then(function (j) {
                    if (j && typeof j.payload === 'string' && j.payload) return j.payload;
                    return null;
                });
            })
            .catch(function () { return null; });
    }

    /* ── Local decode: WASM quirc (demo) or jsQR (classic ESP32). ── */
    function decodeWithWasm(gray, w, h) {
        if (!qrDecodeModule || !qrDecodeModule._wasm_qr_decode) return null;

        var outSize = 2048;
        var grayPtr = qrDecodeModule._malloc(w * h);
        var outPtr  = qrDecodeModule._malloc(outSize);
        if (!grayPtr || !outPtr) {
            if (grayPtr) qrDecodeModule._free(grayPtr);
            if (outPtr)  qrDecodeModule._free(outPtr);
            return null;
        }

        qrDecodeModule.HEAPU8.set(gray, grayPtr);
        var len = qrDecodeModule._wasm_qr_decode(grayPtr, w, h, outPtr, outSize);

        var text = null;
        if (len > 0) {
            var raw = qrDecodeModule.UTF8ToString(outPtr, len);
            /* The QR payload is base32-compressed ("x:base32..."); convert it
               back to hex so the reconstruct flow keeps working with hex. */
            if (qrDecodeModule._wasm_share_from_base32) {
                var hexPtr = qrDecodeModule.ccall('wasm_share_from_base32', 'number', ['string'], [raw]);
                if (hexPtr) {
                    text = qrDecodeModule.UTF8ToString(hexPtr);
                    qrDecodeModule._free(hexPtr);
                }
            } else {
                text = raw;
            }
        }

        qrDecodeModule._free(grayPtr);
        qrDecodeModule._free(outPtr);
        return text;
    }

    /* jsQR is rotation-sensitive, so sweep over scales and rotations. */
    function decodeWithJsQR(src, iw, ih) {
        var targets = [1000, 1500, 600, 400];
        var steps = [
            { dw: iw,  dh: ih,  angle: 0,            tx: 0,   ty: 0   },
            { dw: ih,  dh: iw,  angle:  Math.PI / 2,  tx: ih,  ty: 0   },
            { dw: iw,  dh: ih,  angle:  Math.PI,      tx: iw,  ty: ih  },
            { dw: ih,  dh: iw,  angle: -Math.PI / 2,  tx: 0,   ty: iw  }
        ];

        for (var si = 0; si < steps.length; si++) {
            var s = steps[si];
            for (var ti = 0; ti < targets.length; ti++) {
                var longSide = Math.max(s.dw, s.dh);
                var scale = Math.min(1, targets[ti] / longSide);
                var w = Math.round(s.dw * scale);
                var h = Math.round(s.dh * scale);
                if (w < 60 || h < 60) continue;

                qrCanvas.width  = w;
                qrCanvas.height = h;
                qrCtx.imageSmoothingEnabled = (scale > 0.5);

                if (s.angle === 0) {
                    qrCtx.setTransform(1, 0, 0, 1, 0, 0);
                    qrCtx.drawImage(src, 0, 0, w, h);
                } else {
                    qrCtx.setTransform(
                         Math.cos(s.angle) * scale, Math.sin(s.angle) * scale,
                        -Math.sin(s.angle) * scale, Math.cos(s.angle) * scale,
                        s.tx * scale, s.ty * scale
                    );
                    qrCtx.drawImage(src, 0, 0);
                }

                var code = jsQR(
                    qrCtx.getImageData(0, 0, w, h).data, w, h,
                    { inversionAttempts: 'attemptBoth' }
                );
                if (code && code.data) return code.data.trim();
            }
        }
        return null;
    }

    /* Sync local decode from a source element (camera frame or image). */
    function decodeLocal(src, naturalW, naturalH) {
        if (qrDecodeModule) {
            var frame = drawToGray(src, naturalW, naturalH);
            return decodeWithWasm(frame.gray, frame.w, frame.h);
        }
        if (typeof jsQR === 'function') {
            return decodeWithJsQR(src, naturalW, naturalH);
        }
        return null;
    }

    /* Async: device first (ESP32-S3), WASM fallback (demo). */
    function decodeGray(gray, w, h) {
        if (QR_DECODE_SERVER) {
            return decodeWithServer(gray, w, h).then(function (text) {
                if (text) return text;
                if (qrDecodeModule) return decodeWithWasm(gray, w, h);
                return null;
            });
        }
        return Promise.resolve(null);
    }

    function startQRScan() {
        if (qrStatus) {
            qrStatus.textContent = 'Point the camera at a QR code';
        }

        var lastScanTime = 0;
        var scanInFlight = false;

        // On the ESP32-S3 the ~50 KB uploads are throttled (~250 ms) so they
        // do not saturate the AP link, and requests never stack up.
        function tick() {
            if (!scanInFlight && qrVideo.readyState >= qrVideo.HAVE_ENOUGH_DATA && qrVideo.videoWidth > 0) {
                var now = Date.now();
                if (now - lastScanTime >= QR_SCAN_INTERVAL_MS) {
                    lastScanTime = now;
                    var vw = qrVideo.videoWidth, vh = qrVideo.videoHeight;

                    if (QR_DECODE_SERVER) {
                        var frame = drawToGray(qrVideo, vw, vh);
                        scanInFlight = true;
                        decodeGray(frame.gray, frame.w, frame.h).then(function (text) {
                            scanInFlight = false;
                            if (text) {
                                stopQRScan();
                                fillShareFromQR(text.trim());
                            }
                        });
                    } else {
                        var text = decodeLocal(qrVideo, vw, vh);
                        if (text) {
                            stopQRScan();
                            fillShareFromQR(text.trim());
                            return;
                        }
                    }
                }
            }
            qrAnim = requestAnimationFrame(tick);
        }
        qrAnim = requestAnimationFrame(tick);
    }

    function stopQRScan() {
        if (qrAnim) { cancelAnimationFrame(qrAnim); qrAnim = null; }
        if (qrStream) { qrStream.getTracks().forEach(function (t) { t.stop(); }); qrStream = null; }
        qrVideo.srcObject = null;
        qrOverlay.classList.add('hidden');
    }

    qrClose.addEventListener('click', stopQRScan);
    qrOverlay.addEventListener('click', function (e) {
        if (e.target === qrOverlay) stopQRScan();
    });

    /* ── File / gallery scan ── */
    qrFileInput.addEventListener('change', function () {
        var file = qrFileInput.files[0];
        if (!file) return;
        qrFileInput.value = '';   // allow re-selecting the same file

        var url = URL.createObjectURL(file);
        var img = new Image();
        img.onload = function () {
            URL.revokeObjectURL(url);
            decodeFromImg(img).then(function (text) {
                if (text) { fillShareFromQR(text); }
                else { showToast('No QR code found — try a clearer photo'); }
            });
        };
        img.onerror = function () {
            URL.revokeObjectURL(url);
            showToast('Could not load the image');
        };
        img.src = url;
    });

    /* ── File / gallery decode ──
        Decode on the device (/qr_decode) when available (ESP32-S3), otherwise
        decode locally (WASM quirc demo / jsQR on the classic ESP32). ── */
    function decodeFromImg(img) {
        var iw = img.naturalWidth, ih = img.naturalHeight;
        if (!iw || !ih) return Promise.resolve(null);

        if (QR_DECODE_SERVER) {
            var frame = drawToGray(img, iw, ih);
            return decodeGray(frame.gray, frame.w, frame.h);
        }
        return Promise.resolve(decodeLocal(img, iw, ih));
    }

    function fillShareFromQR(text) {
        var row = shareRows[targetRow];
        if (!row) return;

        var xInput = row.querySelector('.x-input');
        var dInput = row.querySelector('.d-input');

        var colon = text.indexOf(':');
        if (colon !== -1) {
            var xVal = text.substring(0, colon);
            var dVal = text.substring(colon + 1);
            xInput.value = parseInt(xVal, 10) || 1;
            dInput.value = dVal;
        } else {
            dInput.value = text;
        }

        showToast('QR code scanned!');
    }

})();
