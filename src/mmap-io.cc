/*
    Licensed under The MIT License (MIT)
    You will find the full license legal mumbo jumbo in file "LICENSE"

    Copyright (c) 2015 - 2018 Oscar Campbell

    Inspired by Ben Noordhuis module node-mmap - which does the same thing for older node
    versions, sans advise and sync.
*/
#include <nan.h>
#include <errno.h>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include "mman.h"
#else
#include <unistd.h>
#include <sys/mman.h>
#endif

/* #include <future> */

using namespace v8;

// Just a bit more clear as to intent
#define JS_FN(a) NAN_METHOD(a)

// This lib is one of those pieces of code where clarity is better then puny micro opts (in
// comparison to the massive blocking that will occur when the data is first read from disk)
// Since casting `size` to `void*` feels a little "out there" considering that void* may be
// 32b or 64b (or, I dunno, 47b on some quant particle system), we throw this struct in.
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


/*

disables fancy C++17 code for now because of brickwall time consumption of
getting compilers to work on travis, etc...

/ **
 * Make it simpler the next time V8 breaks API's and such with a wrapper fn...
 * /
template <typename T, typename VT>
inline auto get_v(VT v8_value) -> T {
    if constexpr (std::is_same<unsigned long, T>::value) {
        return static_cast<size_t>(Nan::To<int>(v8_value).FromJust());
    } else {
        return Nan::To<T>(v8_value).FromJust();
    }
}

/ **
 * Make it simpler the next time V8 breaks API's and such with a wrapper fn...
 * /
template <typename T, typename VT>
inline auto get_v(VT v8_value, T default_value) -> T {
    if constexpr (std::is_same<unsigned long, T>::value) {
        return static_cast<size_t>(Nan::To<int>(v8_value).FromMaybe(static_cast<int>(default_value)));
    } else {
        return Nan::To<T>(v8_value).FromMaybe(default_value);
    }
}

*/

/**
 * Make it simpler the next time V8 breaks API's and such with a wrapper fn...
 */
template <typename T, typename VT>
inline auto get_v(VT v8_value) -> T {
    return Nan::To<T>(v8_value).FromJust();
}

/**
 * Make it simpler the next time V8 breaks API's and such with a wrapper fn...
 */
template <typename T, typename VT>
inline auto get_v(VT v8_value, T default_value) -> T {
    return Nan::To<T>(v8_value).FromMaybe(default_value);
}

template <typename VT>
inline auto get_obj(VT v8_obj) -> Local<Object> {
    return Nan::To<Object>(v8_obj).ToLocalChecked();
}

JS_FN(mmap_map) {
    Nan::HandleScope();

    if (info.Length() < 4 && info.Length() > 6) {
        return Nan::ThrowError(
            "map() takes 4, 5 or 6 arguments: (size :int, protection :int, flags :int, fd :int [, offset :int [, advise :int]])."
        );
    }

    // Try to be a little (motherly) helpful to us poor clueless developers
    if (!info[0]->IsNumber())    return Nan::ThrowError("mmap: size (arg[0]) must be an integer");
    if (!info[1]->IsNumber())    return Nan::ThrowError("mmap: protection_flags (arg[1]) must be an integer");
    if (!info[2]->IsNumber())    return Nan::ThrowError("mmap: flags (arg[2]) must be an integer");
    if (!info[3]->IsNumber())    return Nan::ThrowError("mmap: fd (arg[3]) must be an integer (a file descriptor)");
    // Offset and advise are optional

    constexpr void* hinted_address  = nullptr;  // Just making things uber-clear...
    const size_t    size            = static_cast<size_t>(get_v<int>(info[0]));
    const int       protection      = get_v<int>(info[1]);
    const int       flags           = get_v<int>(info[2]);
    const int       fd              = get_v<int>(info[3]);
    const size_t    offset          = static_cast<size_t>(get_v<int>(info[4], 0));
    const int       advise          = get_v<int>(info[5], 0);

    char* data = static_cast<char*>( mmap( hinted_address, size, protection, flags, fd, offset) );

    if (data == MAP_FAILED) {
        return Nan::ThrowError((std::string("mmap failed, ") + std::to_string(errno)).c_str());
    }
    else {
        if (advise != 0) {
            auto ret = do_mmap_advice(data, size, advise);
            if (ret) {
                return Nan::ThrowError((std::string("madvise() failed, ") + std::to_string(errno)).c_str());
            }

        //     // Asynchronous read-ahead to minimisze blocking. This
        //     // has worked flawless, but is not necessary, and any
        //     // gains are speculative.
        //     //
        //     // Play with it if you want to.
        //     //
        //     std::async(std::launch::async, [=](){
        //         auto ret = do_mmap_advice(data, size, advise);
        //         if (ret) {
        //             return Nan::ThrowError((std::string("madvise() failed, ") + std::to_string(errno)).c_str());
        //         }
        //         readahead(fd, offset, 1024 * 1024 * 4);
        //     });

        }

        auto map_info = new MMap(data, size);
        Nan::MaybeLocal<Object> buf = node::Buffer::New(
            v8::Isolate::GetCurrent(), data, size, do_mmap_cleanup, static_cast<void*>(map_info));
        if (buf.IsEmpty()) {
            return Nan::ThrowError(std::string("couldn't allocate Node Buffer()").c_str());
        } else {
            info.GetReturnValue().Set(buf.ToLocalChecked());
        }
    }
}

JS_FN(mmap_advise) {
    Nan::HandleScope();

    if (info.Length() != 2 && info.Length() != 4) {
        return Nan::ThrowError(
            "advise() takes 2 or 4 arguments: (buffer :Buffer, advise :int) | (buffer :Buffer, offset :int, length :int, advise :int)."
        );
    }
    if (!info[0]->IsObject())    return Nan::ThrowError("advice(): buffer (arg[0]) must be a Buffer");
    if (!info[1]->IsNumber())    return Nan::ThrowError("advice(): (arg[1]) must be an integer");

    Local<Object>   buf     = get_obj(info[0]); // info[0]->ToObject(); // get_v<Local<Object>>(info[0]);
    char*           data    = node::Buffer::Data(buf);
    size_t          size    = node::Buffer::Length(buf);

    int ret = ([&]() -> int {
        if (info.Length() == 2) {
            int advise = get_v<int>(info[1], 0);
            return do_mmap_advice(data, size, advise);
        }
        else {
            int offset = get_v<int>(info[1], 0);
            int length = get_v<int>(info[2], 0);
            int advise = get_v<int>(info[3], 0);
            return do_mmap_advice(data + offset, length, advise);
        }
    })();

    if (ret) {
        return Nan::ThrowError((std::string("madvise() failed, ") + std::to_string(errno)).c_str());
    }

    //Nan::ReturnUndefined();
}

JS_FN(mmap_incore) {
    Nan::HandleScope();

    if (info.Length() != 1) {
        return Nan::ThrowError(
            "incore() takes 1 argument: (buffer :Buffer) ."
        );
    }

    if (!info[0]->IsObject())    return Nan::ThrowError("advice(): buffer (arg[0]) must be a Buffer");

    Local<Object>   buf     = get_obj(info[0]); // info[0]->ToObject(); // get_v<Local<Object>>(info[0]);
    char*           data    = node::Buffer::Data(buf);
    size_t          size    = node::Buffer::Length(buf);

#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    size_t          page_size = sysinfo.dwPageSize;
#else
    size_t          page_size = sysconf(_SC_PAGESIZE);
#endif

    size_t          needed_bytes = (size+page_size-1) / page_size;
    size_t          pages = size / page_size;

#ifdef __APPLE__
    char*  result_data = static_cast<char *>(malloc(needed_bytes));
#else
    unsigned char*  result_data = static_cast<unsigned char *>(malloc(needed_bytes));
#endif

    if (size % page_size > 0) {
        pages++;
    }

    int ret = mincore(data, size, result_data);

    if (ret) {
        free(result_data);
        if (errno == ENOSYS) {
            return Nan::ThrowError("mincore() not implemented");
        } else {
            return Nan::ThrowError((std::string("mincore() failed, ") + std::to_string(errno)).c_str());
        }
    }

    // Now we want to check all of the pages
    uint32_t pages_mapped = 0;
    uint32_t pages_unmapped = 0;

    for(size_t i = 0; i < pages; i++) {
        if(!(result_data[i] & 0x1)) {
            pages_unmapped++;
        } else {
            pages_mapped++;
        }
    }

    free(result_data);

    v8::Local<v8::Array> arr = Nan::New<v8::Array>(2);
    Nan::Set(arr, 0, Nan::New(pages_unmapped));
    Nan::Set(arr, 1, Nan::New(pages_mapped));
    info.GetReturnValue().Set(arr);
}

JS_FN(mmap_sync_lib_private_) {
    Nan::HandleScope();

    // I barfed at the thought of implementing all variants of info-combos in C++, so
    // the arg-shuffling and checking is done in a ES wrapper function - see "mmap-io.ts"
    if (info.Length() != 5) {
        return Nan::ThrowError(
            "sync() takes 5 arguments: (buffer :Buffer, offset :int, length :int, do_blocking_sync :bool, invalidate_pages_and_signal_refresh_to_consumers :bool)."
        );
    }

    if (!info[0]->IsObject())    return Nan::ThrowError("sync(): buffer (arg[0]) must be a Buffer");

    Local<Object>   buf             = get_obj(info[0]); // info[0]->ToObject(); // get_v<Local<Object>>(info[0]);
    char*           data            = node::Buffer::Data(buf);

    int             offset          = get_v<int>(info[1], 0);
    size_t          length          = get_v<int>(info[2], 0);
    bool            blocking_sync   = get_v<bool>(info[3], false);
    bool            invalidate      = get_v<bool>(info[4], false);
    int             flags           = ( (blocking_sync ? MS_SYNC : MS_ASYNC) | (invalidate ? MS_INVALIDATE : 0) );

    int ret = msync(data + offset, length, flags);

    if (ret) {
        return Nan::ThrowError((std::string("msync() failed, ") + std::to_string(errno)).c_str());
    }
    //Nan::ReturnUndefined();
}


NAN_MODULE_INIT(Init) {
    auto exports = target;

    constexpr auto std_property_attrs = static_cast<PropertyAttribute>(
        ReadOnly | DontDelete
    );

    using JsFnType = decltype(mmap_map);

    auto set_int_prop = [&](const char* key, int val) -> void {
        Nan::DefineOwnProperty(
            exports,
            Nan::New(key).ToLocalChecked(),
            Nan::New(val),
            std_property_attrs
        );
    };

    auto set_fn_prop = [&](const char* key, JsFnType fn) -> void {
        Nan::DefineOwnProperty(
            exports,
            Nan::New<v8::String>(key).ToLocalChecked(),
            Nan::GetFunction(Nan::New<FunctionTemplate>(fn)).ToLocalChecked(),
            std_property_attrs
        );
    };

    set_int_prop("PROT_READ", PROT_READ);
    set_int_prop("PROT_WRITE", PROT_WRITE);
    set_int_prop("PROT_EXEC", PROT_EXEC);
    set_int_prop("PROT_NONE", PROT_NONE);

    set_int_prop("MAP_SHARED", MAP_SHARED);
    set_int_prop("MAP_PRIVATE", MAP_PRIVATE);

#ifdef MAP_NONBLOCK
    set_int_prop("MAP_NONBLOCK", MAP_NONBLOCK);
#endif

#ifdef MAP_POPULATE
    set_int_prop("MAP_POPULATE", MAP_POPULATE);
#endif

    set_int_prop("MADV_NORMAL", MADV_NORMAL);
    set_int_prop("MADV_RANDOM", MADV_RANDOM);
    set_int_prop("MADV_SEQUENTIAL", MADV_SEQUENTIAL);
    set_int_prop("MADV_WILLNEED", MADV_WILLNEED);
    set_int_prop("MADV_DONTNEED", MADV_DONTNEED);

    //set_int_prop("MS_ASYNC", MS_ASYNC);
    //set_int_prop("MS_SYNC", MS_SYNC);
    //set_int_prop("MS_INVALIDATE", MS_INVALIDATE);

#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    set_int_prop("PAGESIZE", sysinfo.dwPageSize);
#else
    set_int_prop("PAGESIZE", sysconf(_SC_PAGESIZE));
#endif


    set_fn_prop("map", mmap_map);
    set_fn_prop("advise", mmap_advise);
    set_fn_prop("incore", mmap_incore);

    // This one is wrapped by a JS-function and deleted from obj to hide from user
    Nan::DefineOwnProperty(
        exports,
        Nan::New<v8::String>("sync_lib_private__").ToLocalChecked(),
        Nan::GetFunction(Nan::New<FunctionTemplate>(mmap_sync_lib_private_)).ToLocalChecked(),
        static_cast<PropertyAttribute>(0)
    );


}

NODE_MODULE(mmap_io, Init)
