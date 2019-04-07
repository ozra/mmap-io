import mmap from "./mmap-io"
import fs from "fs"

const fd = fs.openSync(__filename, "r+")

mmap.map(
    100,
    mmap.PROT_EXEC | mmap.PROT_READ,
    mmap.MAP_SHARED,
    fd,
    0,
    mmap.MADV_SEQUENTIAL
)
