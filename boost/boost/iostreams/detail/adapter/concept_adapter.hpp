// (C) Copyright Jonathan Turkanis 2003.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.)

// See http://www.boost.org/libs/iostreams for documentation.

#ifndef BOOST_IOSTREAMS_DETAIL_CONCEPT_ADAPTER_HPP_INCLUDED
#define BOOST_IOSTREAMS_DETAIL_CONCEPT_ADAPTER_HPP_INCLUDED

#include <boost/config.hpp>                             // SFINAE.
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/detail/adapter/non_blocking_adapter.hpp>
#include <boost/iostreams/detail/call_traits.hpp>
#include <boost/iostreams/detail/char_traits.hpp>
#include <boost/iostreams/detail/dispatch.hpp>
#include <boost/iostreams/detail/error.hpp>
#include <boost/iostreams/detail/streambuf.hpp>        // pubsync.
#include <boost/iostreams/device/null.hpp>
#include <boost/iostreams/traits.hpp>
#include <boost/iostreams/operations.hpp>
#include <boost/mpl/if.hpp>
#include <boost/static_assert.hpp>

#include <boost/iostreams/detail/config/disable_warnings.hpp>  // MSVC.

namespace boost { namespace iostreams { namespace detail {

template<typename Category> struct device_wrapper_impl;
template<typename Category> struct flt_wrapper_impl;

template<typename T>
class concept_adapter {
private:
    typedef typename detail::value_type<T>::type       value_type;
    typedef typename dispatch<T, input, output>::type  input_tag;
    typedef typename dispatch<T, output, input>::type  output_tag;
    typedef typename
            mpl::if_<
                is_device<T>,
                device_wrapper_impl<input_tag>,
                flt_wrapper_impl<input_tag>
            >::type                                    input_impl;
    typedef typename
            mpl::if_<
                is_device<T>,
                device_wrapper_impl<output_tag>,
                flt_wrapper_impl<output_tag>
            >::type                                    output_impl;
    typedef typename
            mpl::if_<
                is_device<T>,
                device_wrapper_impl<any_tag>,
                flt_wrapper_impl<any_tag>
            >::type                                    any_impl;
public:
    typedef typename io_char<T>::type                  char_type;
    typedef typename io_category<T>::type              io_category;

    explicit concept_adapter(const reference_wrapper<T>& ref) : t_(ref.get())
    { BOOST_STATIC_ASSERT(is_std_io<T>::value); }
    explicit concept_adapter(const T& t) : t_(t)
    { BOOST_STATIC_ASSERT(!is_std_io<T>::value); }

    T& operator*() { return t_; }
    T* operator->() { return &t_; }

    std::streamsize read(char_type* s, std::streamsize n)
    { return this->read(s, n, (basic_null_source<char_type>*) 0); }

    template<typename Source>
    std::streamsize read(char_type* s, std::streamsize n, Source* src)
    { return input_impl::read(t_, src, s, n); }

    std::streamsize write(const char_type* s, std::streamsize n)
    { return this->write(s, n, (basic_null_sink<char_type>*) 0); }

    template<typename Sink>
    std::streamsize write(const char_type* s, std::streamsize n, Sink* snk)
    { return output_impl::write(t_, snk, s, n); }

    stream_offset seek( stream_offset off, BOOST_IOS::seekdir way,
                        BOOST_IOS::openmode which )
    { 
        return this->seek( off, way, which, 
                           (basic_null_device<char_type, seekable>*) 0); 
    }

    template<typename Device>
    stream_offset seek( stream_offset off, BOOST_IOS::seekdir way,
                        BOOST_IOS::openmode which, Device* dev )
    { return any_impl::seek(t_, dev, off, way, which); }

    void close(BOOST_IOS::openmode which)
    { this->close(which, (basic_null_device<char_type, seekable>*) 0); }

    template<typename Device>
    void close(BOOST_IOS::openmode which, Device* dev)
    { 
        non_blocking_adapter<Device> nb(*dev);
        any_impl::close(t_, &nb, which); 
    }

    bool flush( BOOST_IOSTREAMS_BASIC_STREAMBUF(char_type,
                BOOST_IOSTREAMS_CHAR_TRAITS(char_type))* sb )
    { 
        bool result = any_impl::flush(t_, sb);
        if (sb && sb->BOOST_IOSTREAMS_PUBSYNC() == -1)
            result = false;
        return result;
    }

    template<typename Locale> // Avoid dependency on <locale>
    void imbue(const Locale& loc) { iostreams::imbue(t_, loc); }

    std::streamsize optimal_buffer_size() const
    { return iostreams::optimal_buffer_size(t_); }
public:
    value_type t_;
};

//------------------Specializations of device_wrapper_impl--------------------//

template<>
struct device_wrapper_impl<any_tag> {
    template<typename Device, typename Dummy>
    static stream_offset 
    seek( Device& dev, Dummy*, stream_offset off, 
          BOOST_IOS::seekdir way, BOOST_IOS::openmode which )
    { 
        typedef typename io_category<Device>::type io_category;
        return seek(dev, off, way, which, io_category()); 
    }

    template<typename Device>
    static stream_offset 
    seek( Device&, stream_offset, BOOST_IOS::seekdir, 
          BOOST_IOS::openmode, any_tag )
    { 
        throw cant_seek(); 
    }

    template<typename Device>
    static stream_offset 
    seek( Device& dev, stream_offset off, 
          BOOST_IOS::seekdir way, BOOST_IOS::openmode which, 
          random_access )
    { 
        return iostreams::seek(dev, off, way, which); 
    }

    template<typename Device, typename Dummy>
    static void close(Device& dev, Dummy*, BOOST_IOS::openmode which)
    { iostreams::close(dev, which); }

    template<typename Device, typename Dummy>
    static bool flush(Device& dev, Dummy*)
    { return iostreams::flush(dev); }
};


template<>
struct device_wrapper_impl<input> : device_wrapper_impl<any_tag>  {
    template<typename Device, typename Dummy>
    static std::streamsize
    read( Device& dev, Dummy*, typename io_char<Device>::type* s,
          std::streamsize n )
    { return iostreams::read(dev, s, n); }

    template<typename Device, typename Dummy>
    static std::streamsize 
    write( Device&, Dummy*, const typename io_char<Device>::type*,
           std::streamsize )
    { throw cant_write(); }
};

template<>
struct device_wrapper_impl<output> {
    template<typename Device, typename Dummy>
    static std::streamsize
    read(Device&, Dummy*, typename io_char<Device>::type*, std::streamsize)
    { throw cant_read(); }

    template<typename Device, typename Dummy>
    static std::streamsize 
    write( Device& dev, Dummy*, const typename io_char<Device>::type* s,
           std::streamsize n )
    { return iostreams::write(dev, s, n); }
};

//------------------Specializations of flt_wrapper_impl--------------------//

template<>
struct flt_wrapper_impl<any_tag> {
    template<typename Filter, typename Device>
    static stream_offset
    seek( Filter& f, Device* dev, stream_offset off,
          BOOST_IOS::seekdir way, BOOST_IOS::openmode which )
    {
        typedef typename io_category<Filter>::type io_category;
        return seek(f, dev, off, way, which, io_category());
    }

    template<typename Filter, typename Device>
    static stream_offset
    seek( Filter&, Device*, stream_offset,
          BOOST_IOS::seekdir, BOOST_IOS::openmode, any_tag )
    { throw cant_seek(); }

    template<typename Filter, typename Device>
    static stream_offset
    seek( Filter& f, Device* dev, stream_offset off,
          BOOST_IOS::seekdir way, BOOST_IOS::openmode which,
          random_access tag )
    {
        typedef typename io_category<Filter>::type io_category;
        return seek(f, dev, off, way, which, tag, io_category());
    }

    template<typename Filter, typename Device>
    static stream_offset
    seek( Filter& f, Device* dev, stream_offset off,
          BOOST_IOS::seekdir way, BOOST_IOS::openmode which,
          random_access, any_tag )
    { return f.seek(*dev, off, way); }

    template<typename Filter, typename Device>
    static stream_offset
    seek( Filter& f, Device* dev, stream_offset off,
          BOOST_IOS::seekdir way, BOOST_IOS::openmode which,
          random_access, two_sequence )
    { return f.seek(*dev, off, way);  }

    template<typename Filter, typename Device>
    static void close(Filter& f, Device* dev, BOOST_IOS::openmode which)
    { iostreams::close(f, *dev, which); }

    template<typename Filter, typename Device>
    static bool flush(Filter& f, Device* dev)
    { return iostreams::flush(f, *dev); }
};

template<>
struct flt_wrapper_impl<input> {
    template<typename Filter, typename Source>
    static std::streamsize
    read( Filter& f, Source* src, typename io_char<Filter>::type* s,
          std::streamsize n )
    { return iostreams::read(f, *src, s, n); }

    template<typename Filter, typename Sink>
    static std::streamsize 
    write( Filter&, Sink*, const typename io_char<Filter>::type*, 
           std::streamsize )
    { throw cant_write(); }
};

template<>
struct flt_wrapper_impl<output> {
    template<typename Filter, typename Source>
    static std::streamsize
    read(Filter&, Source*, typename io_char<Filter>::type*,std::streamsize)
    { throw cant_read(); }

    template<typename Filter, typename Sink>
    static std::streamsize 
    write( Filter& f, Sink* snk, const typename io_char<Filter>::type* s,
           std::streamsize n )
    { return iostreams::write(f, *snk, s, n); }
};

//----------------------------------------------------------------------------//

} } } // End namespaces detail, iostreams, boost.

#include <boost/iostreams/detail/config/enable_warnings.hpp>  // MSVC.

#endif // #ifndef BOOST_IOSTREAMS_DETAIL_CONCEPT_ADAPTER_HPP_INCLUDED
