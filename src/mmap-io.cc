/*
    Licensed under The MIT License (MIT)
    You will find the full license legal mumbo jumbo in file "LICENSE"

    Copyright (c) 2014 - 2015 Oscar Campbell

    Inspired by Ben Noordhuis module node-mmap - which does the same thing for older node
    versions, sans advise and sync.
*/
#include <nan.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include "mman.h"
#else
#include <unistd.h>
#include <sys/mman.h>
#endif

using namespace v8;

// Just a bit more clear and terser
#define JS_FN(a) NAN_METHOD(a)

// Just for the hell of it ;-) - Oscar Campbell
#define IOJS_MODULE(a,b) NODE_MODULE(a, b)

// This lib is one of those pieces of code where clarity is better then puny micro opts (in
// comparison to the massive blocking that will occur when the data is first read from disk)
// Since casting `size` to `void*` feels a little "out there" considering that void* may be
// 32b or 64b (or, I dunno, 47b on some quant particle system), we throw this struct in..
struct MMap {
    MMap(char* data, size_t size) : data(data), size(size) {}
    char*   data = nullptr;
    size_t  size = 0;
};


void do_mmap_cleanup(char* data, void* hint) {
            auto map_info = static_cast<MMap*>(hint);
            munmap(data, map_info->size);
            delete map_info;
}

inline int do_mmap_advice(char* addr, size_t length, int advise) {
            return madvise(static_cast<void*>(addr), length, advise);
}

JS_FN(mmap_map) {
            NanScope();

            if (args.Length() < 4 && args.Length() > 6) {
                return NanThrowError(
                    "map() takes 4, 5 or 6 arguments: (size :int, protection :int, flags :int, fd :int [, offset :int [, advise :int]]).",
                    EINVAL
                );
            }

            // Try to be a little (motherly) helpful to us poor clueless developers
            if (!args[0]->IsNumber())    return NanThrowError("mmap: size (arg[0]) must be an integer", EINVAL);
            if (!args[1]->IsNumber())    return NanThrowError("mmap: protection_flags (arg[1]) must be an integer", EINVAL);
            if (!args[2]->IsNumber())    return NanThrowError("mmap: flags (arg[2]) must be an integer", EINVAL);
            if (!args[3]->IsNumber())    return NanThrowError("mmap: fd (arg[3]) must be an integer (a file descriptor)", EINVAL);
            // Offset and advise are optional

            constexpr void* hinted_address  = nullptr;  // Just making things uber-clear...
            const size_t    size            = args[0]->ToInteger()->Value();   // ToUint64()->Value();
            const int       protection      = args[1]->ToInteger()->Value();
            const int       flags           = args[2]->ToInteger()->Value();
            const int       fd              = args[3]->ToInteger()->Value();
            const off_t     offset          = args[4]->ToInteger()->Value();   // ToInt64()->Value();
            const int       advise          = args[5]->ToInteger()->Value();

            char* data = static_cast<char*>( mmap( hinted_address, size, protection, flags, fd, offset) );

            if (data == MAP_FAILED) {
                return NanThrowError("mmap failed", errno);
            }
            else {
                if (advise != 0) {
                    auto ret = do_mmap_advice(data, size, advise);
                    if (ret) {
                        return NanThrowError("madvise() failed", errno);
                    }
                }

                auto map_info = new MMap(data, size);
                Local<Object> buf = NanNewBufferHandle(data, size, do_mmap_cleanup, static_cast<void*>(map_info));
                NanReturnValue(buf);
            }
}

JS_FN(mmap_advise) {
            NanScope();

            if (args.Length() != 2 && args.Length() != 4) {
                return NanThrowError(
                    "advise() takes 2 or 4 arguments: (buffer :Buffer, advise :int) | (buffer :Buffer, offset :int, length :int, advise :int).",
                    EINVAL
                );
            }
            if (!args[0]->IsObject())    return NanThrowError("advice(): buffer (arg[0]) must be a Buffer", EINVAL);
            if (!args[1]->IsNumber())    return NanThrowError("advice(): (arg[1]) must be an integer", EINVAL);

            Local<Object>   buf     = args[0]->ToObject();
            char*           data    = node::Buffer::Data(buf);
            size_t          size    = node::Buffer::Length(buf);
            int ret;

            if (args.Length() == 2) {
                int advise = args[1]->ToInteger()->Value();
                ret = do_mmap_advice(data, size, advise);
            }
            else {
                int offset = args[1]->ToInteger()->Value();
                int length = args[2]->ToInteger()->Value();
                int advise = args[3]->ToInteger()->Value();
                ret = do_mmap_advice(data + offset, length, advise);
            }
            if (ret) {
                return NanThrowError("madvise() failed", errno);
            }
            NanReturnUndefined();
}

NAN_METHOD(mmap_sync_lib_private_) {
            NanScope();

            // I barfed at the thought of implementing all variants of args-combos in C++, so
            // the arg-shuffling and checking is done in a ES wrapper function - see "mmap-io.ls"
            if (args.Length() != 5) {
                return NanThrowError(
                    "sync() takes 5 arguments: (buffer :Buffer, offset :int, length :int, do_blocking_sync :bool, invalidate_pages_and_signal_refresh_to_consumers :bool).",
                    EINVAL
                );
            }
            if (!args[0]->IsObject())    return NanThrowError("sync(): buffer (arg[0]) must be a Buffer", EINVAL);
            //if (!args[1]->IsNumber())    return NanThrowError("advice(): (arg[1]) must be an integer", EINVAL);

            Local<Object>   buf     = args[0]->ToObject();
            char*           data    = node::Buffer::Data(buf);
            //size_t          size    = node::Buffer::Length(buf);

            int     offset          = args[1]->ToInteger()->Value();
            size_t  length          = args[2]->ToInteger()->Value();
            bool    blocking_sync   = args[3]->ToBoolean()->Value();
            bool    invalidate      = args[4]->ToBoolean()->Value();
            int     flags           = ( (blocking_sync ? MS_SYNC : MS_ASYNC) | (invalidate ? MS_INVALIDATE : 0) );

            int ret = msync(data + offset, length, flags);

            if (ret) {
                return NanThrowError("msync() failed", errno);
            }
            NanReturnUndefined();
}


void Init(Handle<Object> exports, Handle<Object> module) {
            constexpr auto property_attrs = static_cast<PropertyAttribute>(ReadOnly | DontDelete);

            auto set_prop = [&](const char* key, int val) -> void {
                   exports->ForceSet(NanNew(key), NanNew(val), property_attrs);
            };

            set_prop("PROT_READ", PROT_READ);
            set_prop("PROT_WRITE", PROT_WRITE);
            set_prop("PROT_EXEC", PROT_EXEC);
            set_prop("PROT_NONE", PROT_NONE);

            set_prop("MAP_SHARED", MAP_SHARED);
            set_prop("MAP_PRIVATE", MAP_PRIVATE);

            set_prop("MADV_NORMAL", MADV_NORMAL);
            set_prop("MADV_RANDOM", MADV_RANDOM);
            set_prop("MADV_SEQUENTIAL", MADV_SEQUENTIAL);
            set_prop("MADV_WILLNEED", MADV_WILLNEED);
            set_prop("MADV_DONTNEED", MADV_DONTNEED);

            //set_prop("MS_ASYNC", MS_ASYNC);
            //set_prop("MS_SYNC", MS_SYNC);
            //set_prop("MS_INVALIDATE", MS_INVALIDATE);

#ifdef _WIN32
            SYSTEM_INFO sysinfo;
            GetSystemInfo(&sysinfo);
            set_prop("PAGESIZE", sysinfo.dwPageSize);
#else
            set_prop("PAGESIZE", sysconf(_SC_PAGESIZE));
#endif

            exports->ForceSet(NanNew("map"), NanNew<FunctionTemplate>(mmap_map)->GetFunction(), property_attrs);
            exports->ForceSet(NanNew("advise"), NanNew<FunctionTemplate>(mmap_advise)->GetFunction(), property_attrs);

            // This one is wrapped by a JS-function and deleted from obj to hide from user
            // *TODO* perhaps call is sync so that we simply can drop in to the sync-name in JS, no deleting
            exports->ForceSet(NanNew("sync_lib_private__"), NanNew<FunctionTemplate>(mmap_sync_lib_private_)->GetFunction(), PropertyAttribute::None);

}

IOJS_MODULE(mmap_io, Init)
