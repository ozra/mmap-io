const mmap_lib_raw_ = require("../build/Release/mmap-io")

type FileDescriptor = number

type MapProtectionFlags =
    | MmapIo["PROT_NONE"] // 0
    | MmapIo["PROT_READ"] // 1
    | MmapIo["PROT_WRITE"] // 2
    | MmapIo["PROT_EXEC"] // 4
    | 3 // R+W
    | 5 // R+X
    | 6 // W+X
    | 7 // R+W+X

// making `map` a wrapper around the C++ `map`-implementation and allowing an
// array of flags would be clean, perfectly literally typed, and the dirtier
// binary-or can be done in the wrapper.
//
// type MapProtectionFlagsList = Array<
//     | MmapIo["PROT_NONE"]
//     | MmapIo["PROT_READ"]
//     | MmapIo["PROT_WRITE"]
//     | MmapIo["PROT_EXEC"]
// >

type MapFlags =
    | MmapIo["MAP_PRIVATE"]
    | MmapIo["MAP_SHARED"]
    | MmapIo["MAP_NONBLOCK"]
    | MmapIo["MAP_POPULATE"]
    | number

type MapAdvise =
    | MmapIo["MADV_NORMAL"]
    | MmapIo["MADV_RANDOM"]
    | MmapIo["MADV_SEQUENTIAL"]
    | MmapIo["MADV_WILLNEED"]
    | MmapIo["MADV_DONTNEED"]

type MmapIo = {
    map(
        size: number,
        protection: MapProtectionFlags,
        flags: MapFlags,
        fd: FileDescriptor,
        offset?: number,
        advise?: MapAdvise
    ): Buffer

    advise(
        buffer: Buffer,
        offset: number,
        length: number,
        advise: MapAdvise
    ): void
    advise(buffer: Buffer, advise: MapAdvise): void

    /// Returns tuple of [ unmapped-pages-count, mapped-pages-count ]
    incore(buffer: Buffer): [number, number]

    sync(
        buffer: Buffer,
        offset?: number,
        size?: number,
        blocking_sync?: boolean,
        invalidate_pages?: boolean
    ): void

    sync(
        buffer: Buffer,
        blocking_sync: boolean,
        invalidate_pages?: boolean
    ): void

    readonly PROT_READ: 1
    readonly PROT_WRITE: 2
    readonly PROT_EXEC: 4
    readonly PROT_NONE: 0
    readonly MAP_SHARED: 1
    readonly MAP_PRIVATE: 2
    readonly MAP_NONBLOCK: 65536
    readonly MAP_POPULATE: 32768
    readonly MADV_NORMAL: 0
    readonly MADV_RANDOM: 1
    readonly MADV_SEQUENTIAL: 2
    readonly MADV_WILLNEED: 3
    readonly MADV_DONTNEED: 4

    readonly PAGESIZE: number
}

// snatch the raw C++-sync func
const raw_sync_fn_ = mmap_lib_raw_.sync_lib_private__

// Hide the original C++11 func from users
delete mmap_lib_raw_.sync_lib_private__

// Take care of all the param juggling here instead of in C++ code, by making
// some overloads, and doing some argument defaults /ozra
mmap_lib_raw_.sync = function(
    buf: Buffer,
    par_a?: any,
    par_b?: any,
    par_c?: any,
    par_d?: any
): void {
    if (typeof par_a === "boolean") {
        raw_sync_fn_(buf, 0, buf.length, par_a, par_b || false)
    } else {
        raw_sync_fn_(
            buf,
            par_a || 0,
            par_b || buf.length,
            par_c || false,
            par_d || false
        )
    }
}

// mmap_lib_raw_.sync = sync_

const mmap = mmap_lib_raw_ as MmapIo
module.exports = mmap
export default mmap
