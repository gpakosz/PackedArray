# PackedArray: random access array of tightly packed unsigned integers

## TLDR

*PackedArray comes to the rescue when you're in a desperate need for an uint9_t
or uint17_t array.*

## What?

When you want to hold an unordered sequence of unsigned integers into memory,
the C programming language lets you choose among 4 data types:

- `uint8_t`
- `uint16_t`
- `uint32_t`
- `uint64_t`

If your numbers are within the [0, 100000] range, only 17 bits per integer are
needed since 2<sup>17</sup> = 131072. However, you can't use an array of
`uint16_t` because 16 bits are not enough to store numbers between 65536 and
100000. When you use the next available type, `uint32_t`, you're wasting 15 bits
per integer which represents a 47% overhead in terms of storage requirements.
                                                         
`PackedArray` saves memory by packing integers/items together at the bit-level:

<table class="monospace">
  <tr>
    <td colspan="32">b0</td>
    <td colspan="32">b1</td>
    <td colspan="32">b2</td>
    <td style="border-style: dashed; border-right: none;">...</td>
  </tr>
  <tr>
    <td colspan="9">i0</td>
    <td colspan="9">i1</td>
    <td colspan="9">i2</td>
    <td colspan="9">i3</td>
    <td colspan="9">i4</td>
    <td colspan="9">i5</td>
    <td colspan="9">i6</td>
    <td colspan="9">i7</td>
    <td colspan="9">i8</td>
    <td colspan="9">i9</td>
    <td style="border-style: dashed; border-right: none;">...</td>
  </tr>
</table>

A `PackedArray` is backed by an `uint32_t` buffer. Several items end up being
stored inside the same buffer cell, e.g. i0, i1, and i2. Some items span two
buffer cells, e.g. i3, and i7. `PackedArray` is responsible for
encoding/decoding items into/from the storage buffer.

`PackedArraySIMD` is a `PackedArray` variant that makes use of SSE2 or NEON
instructions.

Going SIMD processes integers 4 by 4 but imposes an interleaved layout in the
storage buffer.
 
`PackedArraySIMD` interleaved layout, 13 bits per item:

<table class="monospace">
  <tr>
    <td colspan="32">b0</td>
    <td colspan="32">b1</td>
    <td colspan="32">b2</td>
    <td colspan="32">b3</td>
    <td style="border-style: dashed; border-right: none;">...</td>
  </tr>
  <tr>
    <td colspan="13">i0</td>
    <td colspan="13">i4</td>
    <td colspan="6">i8a</td>
    <td colspan="13">i1</td>
    <td colspan="13">i5</td>
    <td colspan="6">i9a</td>
    <td colspan="13">i2</td>
    <td colspan="13">i6</td>
    <td colspan="6">i10a</td>
    <td colspan="13">i3</td>
    <td colspan="13">i7</td>
    <td colspan="6">i11a</td>
    <td style="border-style: dashed;">i8b</td>
    <td style="border-style: dashed; border-right: none;">...</td>
  </tr>
</table>

As a consequence, the data layout of `PackedArraySIMD` isn't compatible with its
non SIMD counterpart. In other words, you cannot use `PackedArray` to unpack
data packed with `PackedArraySIMD` or the other way around.

It is also worth noting the implementations of `PackedArraySIMD_pack` and
`PackedArraySIMD_unpack` require more plumbing than their non-SIMD counterparts.
Additional computations are needed to find out and adjust a data window that can
be processed 4 by 4 with SIMD instructions.

`PackedArray` and `PackedArraySIMD` are released under the WTFPL v2 license.

For more information, see the [PackedArray announcement on my personal website].

[PackedArray announcement on my personal website]: http://pempek.net/articles/2013/08/03/packedarray-random-access-array-tightly-packed-unsigned-integers/

## Why?

`PackedArray` is designed as a drop-in replacement for an unsigned integer
array. I couldn't find such a data structure in the wild, so I implemented one.

Instead of writing:

    uint32_t* a = (uint32_t*)malloc(sizeof(uint32_t) * count);
    ...
    value = a[i];
    ...
    a[j] = value;

You write:

    PackedArray* a = PackedArray_create(bitsPerItem, count);
    ...
    value = PackedArray_get(a, i);
    ...
    PackedArray_set(a, j, value);

The `PackedArray_computeBitsPerItem` helper scans a `uint32_t` array and returns
the number of bits needed to create a `PackedArray` capable of holding its
content.

There are also `PackedArray_pack` and `PackedArray_unpack` that operate on
several items in a row. Those two could really have been named
`PackedArray_write` and `PackedArray_read` but I decided "pack" / "unpack"
conveys better something is happening under the hood.

    // bulk packing / unpacking
    PackedArray_pack(a, j, in, count);
    PackedArray_unpack(a, j, out, count);

    // the following are semantically equivalent
    PackedArray_set(a, j, value);
    PackedArray_pack(a, j, &value, 1);

    value = PackedArray_get(a, i);
    PackedArray_unpack(a, i, &value, 1);

--------------------------------------------------------------------------------

## Compiling

In order to use `PackedArray` or `PackedArraySIMD` in your own project, you just
have to bring in the two `PackedArray.h` and `PackedArray.c` (or
`PackedArraySIMD.c`) files. It's that simple.

You can customize `PackedArray.c`'s behavior by defining the following macros:

- `PACKEDARRAY_ASSERT`
- `PACKEDARRAY_MALLOC`
- `PACKEDARARY_FREE`

You can customize `PackedArraySIMD.c`'s behavior by defining the following
macros:

- `PACKEDARRAY_ASSERT`
- `PACKEDARRAY_ALIGNED_MALLOC`
- `PACKEDARARY_FREE`

`PackedArray.c` and `PackedArraySIMD.c` can compile themselves into either a
test program or a micro-benchmark. For that, you have to use one of the
following preprocessor directives:

- `PACKEDARRAY_SELF_TEST`
- `PACKEDARRAY_SELF_BENCH`

For example, from command line:

    $ cc -o PackedArraySelfTest -DPACKEDARRAY_SELF_TEST -O2 -g PackedArray.c
    $ cc -o PackedArraySelfBench -DPACKEDARRAY_SELF_BENCH -DNDEBUG -O2 -g PackedArray.c

    $ cc -o PackedArraySIMDSelfTest -DPACKEDARRAY_SELF_TEST -O2 -g PackedArraySIMD.c
    $ cc -o PackedArraySIMDSelfBench -DPACKEDARRAY_SELF_BENCH -DNDEBUG -O2 -g PackedArraySIMD.c

### Compiling for Windows

There is a Visual Studio 2012 solution in the `_win-vs11/` folder.

### Compiling for Linux or Mac

There is a GNU Make 3.81 `MakeFile` in the `_gnu-make/` folder:

    $ make -C _gnu-make/

### Compiling for Mac

See above if you want to compile from command line. Otherwise there is an Xcode
project located in the `_mac-xcode/` folder.

### Compiling for iOS

There is an Xcode project located in the `_ios-xcode/` folder.

If you prefer compiling from command line and deploying to a jailbroken device
through SSH, launch one of the following from your terminal prompt.

    $ mkdir -p bin/ios
    $ /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS6.1.sdk -arch armv7 -o bin/ios/PackedArraySelfTest -DPACKEDARRAY_SELF_TEST -DNDEBUG -O2 PackedArray.c
    $ codesign -s "iPhone Developer" ./bin/ios/PackedArraySelfTest

    $ mkdir -p bin/ios
    $ /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS6.1.sdk -arch armv7 -o bin/ios/PackedArraySelfBench -DPACKEDARRAY_SELF_BENCH -DNDEBUG -O2 PackedArray.c
    $ codesign -s "iPhone Developer" ./bin/ios/PackedArraySelfBench

    $ mkdir -p bin/ios
    $ /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS6.1.sdk -arch armv7 -o bin/ios/PackedArraySIMDSelfTest -DPACKEDARRAY_SELF_TEST -DNDEBUG -O2 PackedArraySIMD.c
    $ codesign -s "iPhone Developer" ./bin/ios/PackedArraySIMDSelfTest

    $ mkdir -p bin/ios
    $ /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS6.1.sdk -arch armv7 -o bin/ios/PackedArraySIMDSelfBench -DPACKEDARRAY_SELF_BENCH -DNDEBUG -O2 PackedArraySIMD.c
    $ codesign -s "iPhone Developer" ./bin/ios/PackedArraySIMDSelfBench

### Compiling for Android

You will have to install the Android NDK, and point the `$NDK_ROOT` environment
variable to the NDK path: e.g. `export NDK_ROOT = /opt/android-ndk` (without a
trailing `/` character).

Next, the easy way is to make a standalone Android toolchain with the following
command:

    $ $NDK_ROOT/build/tools/make-standalone-toolchain.sh --system=$(uname -s | tr [A-Z] [a-z])-$(uname -m) --platform=android-3 --toolchain=arm-linux-androideabi-clang3.3 --install-dir=/tmp/android-clang

Now you can compile the self test and self benchmark programs by running:

    $ make -C _gnu-make/ binsubdir=android CC=/tmp/android-clang/bin/clang CFLAGS='-march=armv7-a -mfloat-abi=softfp -mfpu=neon -O2'

--------------------------------------------------------------------------------

## Implementation details, what the hell is going on?

First, in `PackedArray.c` or `PackedArraySIMD.c`, everything that comes below
the `- 8< ----` marker is the code for the self test and self micro-benchmark
programs and can be discarded if you really want to:

If you want to cut down your anxiety, you can use the provided GNU Makefile and
invoke:

    $ make -C _gnu-make/ cut

This produces the `PackedArray.cut.c` and `PackedArraySIMD.cut.c` files.

You may also be troubled by `PackedArray.c` and `PackedArraySIMD.c` including
themselves with `#include PACKEDARRAY_SELF`. By combining preprocessing tricks
and including themselves, `PackedArray.c` and `PackedArraySIMD.c`
"generate the code" for the unrolled pack and unpack implementations.

By default `PACKEDARRAY_SELF` is defined to `"PackedArray.c"` which assumes the
compiler is going to look for the file in the same directory as the file from
which the `#include` statement is being evaluated. This helps compiling when the
build system refers to the source files with relative paths. Depending on your
compiler/build system combination you may want to override `PACKEDARRAY_SELF` to
`__FILE__`.

If you want to see the generated code, you can use the provided GNU Makefile and
invoke:

    $ make -C _gnu-make/ preprocess

This produces the `PackedArray.pp.c` and `PackedArraySIMD.pp.c` files.


--------------------------------------------------------------------------------

If you find `PackedArray` or `PackedArraySIMD` useful and decide to use it in
your own projects please drop me a line [@gpakosz].

If you use it in a commercial project, consider using [Gittip].

[@gpakosz]: https://twitter.com/gpakosz
[Gittip]: https://www.gittip.com/gpakosz/
