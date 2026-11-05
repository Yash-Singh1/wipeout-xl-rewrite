# Third-party code

## Monkey's Audio

`platform/ape.c` (decoding the music tracks of copies kept as `.ape`) is adapted from the Monkey's Audio SDK:

Copyright 2000-2026 Matthew T. Ashland. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
   disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
   disclaimer in the documentation and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
   products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## Adaptive downsampling

The Adaptive downsampling style in `platform/psx/gpu_gl.c` is inspired by the
[adaptive downsampling feature in DuckStation](https://github.com/stenzek/duckstation#features).
Credit goes to the DuckStation project and its contributors for that approach.

This project uses independently written variance-guided reduction, detail-weight
filtering and reconstruction shaders. No DuckStation source code is incorporated.
The result follows the same goal of smoothing enlarged flat pixels while preserving
fine rendered detail; it does not promise identical output to DuckStation. Box
downsampling uses conventional area averaging.
