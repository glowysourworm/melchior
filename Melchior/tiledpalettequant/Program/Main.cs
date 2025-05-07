using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace tiledpalettequant.Program
{
    /// <summary>
    /// This is the "program code" for the palette quantizer. 
    /// </summary>
    public static class Main
    {
        public static bool Validate(Inputs settings)
        {
            return false;

            /*

                PRE RUN VAILDATION

                if (totalPaletteColors > 256) {
                    quantizedImageDownload.download =
                        sourceImageName + settingsStr + ".png";
                }
                else {
                    quantizedImageDownload.download =
                        sourceImageName + settingsStr + ".bmp";
                }

                function bmpToDataURL(width, height, paletteData, colorIndexes) {
                    const bmpFileSize = 54 + paletteData.length + colorIndexes.length;
                    const bmpData = new Uint8ClampedArray(bmpFileSize);
                    bmpData[0] = 66;
                    bmpData[1] = 77;
                    write32Le(bmpData, 2, bmpFileSize);
                    write32Le(bmpData, 6, 0);
                    write32Le(bmpData, 0xa, 54 + paletteData.length);
                    write32Le(bmpData, 0xe, 40);
                    write32Le(bmpData, 0x12, width);
                    write32Le(bmpData, 0x16, height);
                    write16Le(bmpData, 0x1a, 1);
                    write16Le(bmpData, 0x1c, 8);
                    write32Le(bmpData, 0x1e, 0);
                    write32Le(bmpData, 0x22, colorIndexes.length);
                    write32Le(bmpData, 0x26, 2835);
                    write32Le(bmpData, 0x2a, 2835);
                    write32Le(bmpData, 0x2e, 256);
                    write32Le(bmpData, 0x32, 0);
                    for (let i = 0; i < paletteData.length; i++) {
                        bmpData[i + 54] = paletteData[i];
                    }
                    const imageDataAddress = 54 + paletteData.length;
                    for (let i = 0; i < colorIndexes.length; i++) {
                        bmpData[i + imageDataAddress] = colorIndexes[i];
                    }
                    return "data:image/bmp;base64," + uint8ToBase64(bmpData);
                }

                function uint8ToBase64(arr) {
                    return btoa(Array(arr.length)
                        .fill("")
                        .map((_, i) => String.fromCharCode(arr[i]))
                        .join(""));
                }
                function write32Le(bmpData, index, value) {
                    bmpData[index] = value % 256;
                    value = Math.floor(value / 256);
                    bmpData[index + 1] = value % 256;
                    value = Math.floor(value / 256);
                    bmpData[index + 2] = value % 256;
                    value = Math.floor(value / 256);
                    bmpData[index + 3] = value % 256;
                }
                function write16Le(bmpData, index, value) {
                    bmpData[index] = value % 256;
                    value = Math.floor(value / 256);
                    bmpData[index + 1] = value % 256;
                }


            */
        }

        public static void Run(Inputs settings)
        {
            /*
            function quantizeImage(image) {
                console.log(quantizationOptions);
                const t0 = performance.now();
                const reducedImageData = {
                    width: image.width,
                    height: image.height,
                    data: new Uint8ClampedArray(image.data.length),
                };
                const useDither = quantizationOptions.dither !== Dither.Off;
                if (useDither) {
                    for (let i = 0; i < image.data.length; i++) {
                        reducedImageData.data[i] = image.data[i];
                    }
                }
                else {
                    for (let i = 0; i < image.data.length; i++) {
                        reducedImageData.data[i] = toNbit(image.data[i], quantizationOptions.bitsPerChannel);
                    }
                }
                const tiles = extractTiles(reducedImageData);
                let avgPixelsPerTile = 0;
                for (const tile of tiles) {
                    avgPixelsPerTile += tile.colors.length;
                }
                avgPixelsPerTile /= tiles.length;
                console.log("Colors per tile: " + avgPixelsPerTile.toFixed(2));
                const pixels = extractAllPixels(tiles);
                const randomShuffle = new RandomShuffle(pixels.length);
                const showProgress = true;
                let iterations = quantizationOptions.fractionOfPixels * pixels.length;
                let alpha = 0.3;
                let finalAlpha = 0.05;
                const meanSquareErr = meanSquareError;
                if (quantizationOptions.dither === Dither.Slow) {
                    // meanSquareErr = meanSquareErrorDither;
                    iterations /= 5;
                    alpha = 0.1;
                    finalAlpha = 0.02;
                }
                const minColorFactor = 0.5;
                const minPaletteFactor = 0.5;
                const replaceIterations = 10;
                const useMin = true;
                const prog = [25, 65, 90, 100];
                if (quantizationOptions.dither != Dither.Off) {
                    prog[3] = 94;
                }
                let palettes = colorQuantize1Color(tiles, pixels, randomShuffle);
                let startIndex = 2;
                if (quantizationOptions.colorZeroBehaviour === ColorZeroBehaviour.Shared) {
                    startIndex += 1;
                }
                let endIndex = quantizationOptions.colorsPerPalette;
                if (quantizationOptions.colorZeroBehaviour ===
                    ColorZeroBehaviour.TransparentFromColor ||
                    quantizationOptions.colorZeroBehaviour ===
                        ColorZeroBehaviour.TransparentFromTransparent) {
                    endIndex -= 1;
                }
                updateProgress(prog[0] / quantizationOptions.numPalettes);
                updatePalettes(palettes, false);
                if (showProgress)
                    updateQuantizedImage(quantizeTiles(palettes, reducedImageData, false));
                for (let numColors = startIndex; numColors <= endIndex; numColors++) {
                    expandPalettesByOneColor(palettes, tiles, pixels, randomShuffle);
                    updateProgress((prog[0] * numColors) / quantizationOptions.colorsPerPalette);
                    updatePalettes(palettes, false);
                    if (showProgress)
                        updateQuantizedImage(quantizeTiles(palettes, reducedImageData, false));
                }
                let minMse = meanSquareErr(palettes, tiles);
                let minPalettes = structuredClone(palettes);
                for (let i = 0; i < replaceIterations; i++) {
                    palettes = replaceWeakestColors(palettes, tiles, minColorFactor, minPaletteFactor, true);
                    for (let iteration = 0; iteration < iterations; iteration++) {
                        const nextPixel = pixels[randomShuffle.next()];
                        movePalettesCloser(palettes, nextPixel, alpha);
                    }
                    const mse = meanSquareErr(palettes, tiles);
                    if (mse < minMse) {
                        minMse = mse;
                        minPalettes = structuredClone(palettes);
                    }
                    updateProgress(prog[0] + ((prog[1] - prog[0]) * (i + 1)) / replaceIterations);
                    updatePalettes(palettes, false);
                    if (showProgress) {
                        if (useMin && i === replaceIterations - 1) {
                            updateQuantizedImage(quantizeTiles(minPalettes, reducedImageData, false));
                        }
                        else {
                            updateQuantizedImage(quantizeTiles(palettes, reducedImageData, false));
                        }
                    }
                    console.log("MSE: " + mse.toFixed(0));
                    // console.log((performance.now() - t1).toFixed(0) + " ms");
                }
                if (useMin) {
                    palettes = minPalettes;
                }
                if (!useDither)
                    palettes = reducePalettes(palettes, quantizationOptions.bitsPerChannel);
                const finalIterations = iterations * 10;
                let nextUpdate = iterations;
                for (let iteration = 0; iteration < finalIterations; iteration++) {
                    const nextPixel = pixels[randomShuffle.next()];
                    movePalettesCloser(palettes, nextPixel, finalAlpha);
                    if (iteration >= nextUpdate) {
                        nextUpdate += iterations;
                        updateProgress(prog[1] + ((prog[2] - prog[1]) * iteration) / finalIterations);
                        updatePalettes(palettes, false);
                    }
                }
                console.log("Normal final: " + meanSquareError(palettes, tiles).toFixed(0));
                console.log("Dither final: " + meanSquareErrorDither(palettes, tiles).toFixed(0));
                updateProgress(prog[2]);
                updatePalettes(palettes, false);
                if (!useDither) {
                    palettes = reducePalettes(palettes, quantizationOptions.bitsPerChannel);
                    for (let i = 0; i < 3; i++) {
                        palettes = kMeans(palettes, tiles);
                        updateProgress(prog[2] + ((prog[3] - prog[2]) * (i + 1)) / 3);
                        updatePalettes(palettes, false);
                    }
                }
                palettes = reducePalettes(palettes, quantizationOptions.bitsPerChannel);
                updatePalettes(palettes, true);
                updateQuantizedImage(quantizeTiles(palettes, reducedImageData, useDither));
                console.log("> MSE: " + meanSquareError(palettes, tiles).toFixed(2));
                console.log(`> Time: ${((performance.now() - t0) / 1000).toFixed(2)} sec`);
            }
            */




        }
    }
}
