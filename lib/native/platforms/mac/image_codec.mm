//
// Implements PNG and JPEG image I/O with macOS ImageIO.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>

#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

#include "../../image_codec.h"

namespace
{
    bool is_supported_input(const std::uint8_t *data, std::size_t size) {
        const bool png = size >= 8 &&
            std::memcmp(data, "\x89PNG\r\n\x1a\n", 8) == 0;
        const bool jpeg = size >= 2 && data[0] == 0xff && data[1] == 0xd8;
        return png || jpeg;
    }

    std::uint8_t unpremultiply(std::uint8_t value, std::uint8_t alpha) {
        if (alpha == 0)
            return 0;
        return static_cast<std::uint8_t>(
            (static_cast<unsigned>(value) * 255U + alpha / 2U) / alpha);
    }
}

namespace native::detail
{
    decoded_image decode_image(
        const std::uint8_t *data,
        std::size_t size) {
        if (!is_supported_input(data, size))
            throw std::runtime_error(
                "macOS image codec: input is not PNG or JPEG");

        CFDataRef encoded = CFDataCreate(
            kCFAllocatorDefault,
            data,
            static_cast<CFIndex>(size));
        if (!encoded)
            throw std::bad_alloc();
        CGImageSourceRef source = CGImageSourceCreateWithData(encoded, nullptr);
        CFRelease(encoded);
        if (!source)
            throw std::runtime_error("macOS image codec: decode failed");

        CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
        CFRelease(source);
        if (!image)
            throw std::runtime_error("macOS image codec: decode failed");

        const std::size_t width = CGImageGetWidth(image);
        const std::size_t height = CGImageGetHeight(image);
        if (width == 0 || height == 0 ||
            width > static_cast<std::size_t>(
                std::numeric_limits<coord>::max()) ||
            height > static_cast<std::size_t>(
                std::numeric_limits<coord>::max())) {
            CGImageRelease(image);
            throw std::runtime_error(
                "macOS image codec: dimensions exceed native image limits");
        }

        decoded_image decoded;
        decoded.width = static_cast<dim>(width);
        decoded.height = static_cast<dim>(height);
        decoded.pixels = std::make_unique<rgba[]>(width * height);

        CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(
            decoded.pixels.get(),
            width,
            height,
            8,
            width * sizeof(rgba),
            color_space,
            static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) |
                static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big));
        CGColorSpaceRelease(color_space);
        if (!context) {
            CGImageRelease(image);
            throw std::runtime_error(
                "macOS image codec: unable to create pixel buffer");
        }

        CGContextDrawImage(
            context,
            CGRectMake(0, 0, width, height),
            image);
        CGContextRelease(context);
        CGImageRelease(image);

        for (std::size_t index = 0; index < width * height; ++index) {
            rgba &pixel = decoded.pixels[index];
            pixel.r = unpremultiply(pixel.r, pixel.a);
            pixel.g = unpremultiply(pixel.g, pixel.a);
            pixel.b = unpremultiply(pixel.b, pixel.a);
        }
        return decoded;
    }

    std::vector<std::uint8_t> encode_image(
        image_format format,
        const rgba *pixels,
        dim width,
        dim height,
        int jpeg_quality) {
        CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
        CGDataProviderRef provider = CGDataProviderCreateWithData(
            nullptr,
            pixels,
            static_cast<std::size_t>(width) * height * sizeof(rgba),
            nullptr);
        CGImageRef image = CGImageCreate(
            width,
            height,
            8,
            32,
            static_cast<std::size_t>(width) * sizeof(rgba),
            color_space,
            static_cast<CGBitmapInfo>(kCGImageAlphaLast) |
                static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big),
            provider,
            nullptr,
            false,
            kCGRenderingIntentDefault);
        CGDataProviderRelease(provider);
        CGColorSpaceRelease(color_space);
        if (!image)
            throw std::runtime_error(
                "macOS image codec: unable to create image");

        CFMutableDataRef encoded = CFDataCreateMutable(kCFAllocatorDefault, 0);
        const CFStringRef type = format == image_format::png
            ? CFSTR("public.png")
            : CFSTR("public.jpeg");
        CGImageDestinationRef destination =
            CGImageDestinationCreateWithData(encoded, type, 1, nullptr);
        if (!destination) {
            CFRelease(encoded);
            CGImageRelease(image);
            throw std::runtime_error(
                "macOS image codec: requested encoder is unavailable");
        }

        CFDictionaryRef properties = nullptr;
        CFNumberRef quality = nullptr;
        if (format == image_format::jpeg) {
            const double fraction = static_cast<double>(jpeg_quality) / 100.0;
            quality = CFNumberCreate(
                kCFAllocatorDefault,
                kCFNumberDoubleType,
                &fraction);
            const void *keys[] = {kCGImageDestinationLossyCompressionQuality};
            const void *values[] = {quality};
            properties = CFDictionaryCreate(
                kCFAllocatorDefault,
                keys,
                values,
                1,
                &kCFTypeDictionaryKeyCallBacks,
                &kCFTypeDictionaryValueCallBacks);
        }

        CGImageDestinationAddImage(destination, image, properties);
        const bool finalized = CGImageDestinationFinalize(destination);
        if (properties)
            CFRelease(properties);
        if (quality)
            CFRelease(quality);
        CFRelease(destination);
        CGImageRelease(image);
        if (!finalized) {
            CFRelease(encoded);
            throw std::runtime_error("macOS image codec: encode failed");
        }

        const auto *bytes = CFDataGetBytePtr(encoded);
        const CFIndex length = CFDataGetLength(encoded);
        std::vector<std::uint8_t> result(bytes, bytes + length);
        CFRelease(encoded);
        return result;
    }
}
