#ifndef __LIBCDOC_UTILS_H__
#define __LIBCDOC_UTILS_H__

#include "Io.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

namespace libcdoc {

std::string toBase64(const uint8_t *data, size_t len);
static std::string toBase64(const std::vector<uint8_t> data) {
    return toBase64(data.data(), data.size());
}

template <typename F>
static std::string toHex(const F &data)
{
    std::stringstream os;
    os << std::hex << std::uppercase << std::setfill('0');
    for(const auto &i: data)
        os << std::setw(2) << (static_cast<int>(i) & 0xFF);
    return os.str();
}

static std::vector<uint8_t>
fromHex(const std::string_view& hex) {
    std::vector<uint8_t> val(hex.size() / 2);
    char c[3] = {0};
    for (size_t i = 0; i < (hex.size() & 0xfffffffe); i += 2) {
        std::copy(hex.cbegin() + i, hex.cbegin() + i + 2, c);
        val[i / 2] = (uint8_t) strtol(c, NULL, 16);
    }
    return std::move(val);
}

static std::vector<std::string>
split (const std::string &s, char delim = ':') {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;
    while (getline (ss, item, delim)) {
        result.push_back (item);
    }
    return result;
}

static std::vector<uint8_t>
readAllBytes(const std::string_view filename)
{
    std::filesystem::path keyFilePath(filename);
    if (!std::filesystem::exists(keyFilePath)) {
        std::cerr << "readAllBytes(): File '" << filename << "' does not exist" << std::endl;
        return {};
    }
    std::ifstream keyStream(keyFilePath, std::ios_base::in | std::ios_base::binary);
    if (!keyStream) {
        std::cerr << "readAllBytes(): Opening '" << filename << "' failed." << std::endl;
        return {};
    }

    // Determine the file size
    keyStream.seekg(0, std::ios_base::end);
    size_t length = keyStream.tellg();

    // Read the file
    std::vector<uint8_t> dst(length);
    keyStream.seekg(0);
    keyStream.read(reinterpret_cast<std::ifstream::char_type *>(dst.data()), length);
    return dst;
}

int parseURL(const std::string& url, std::string& host, int& port, std::string& path);

std::string urlEncode(const std::string_view &src);
std::string urlDecode(std::string &src);

#ifdef _WIN32
#include <Windows.h>

static std::wstring toWide(UINT codePage, const std::string &in)
{
	std::wstring result;
	if(in.empty())
		return result;
	int len = MultiByteToWideChar(codePage, 0, in.data(), int(in.size()), nullptr, 0);
	result.resize(size_t(len), 0);
	len = MultiByteToWideChar(codePage, 0, in.data(), int(in.size()), &result[0], len);
	return result;
}

static std::string toMultiByte(UINT codePage, const std::wstring &in)
{
	std::string result;
	if(in.empty())
		return result;
	int len = WideCharToMultiByte(codePage, 0, in.data(), int(in.size()), nullptr, 0, nullptr, nullptr);
	result.resize(size_t(len), 0);
	len = WideCharToMultiByte(codePage, 0, in.data(), int(in.size()), &result[0], len, nullptr, nullptr);
	return result;
}
#endif

static std::string toUTF8(const std::string &in)
{
#ifdef _WIN32
	return toMultiByte(CP_UTF8, toWide(CP_ACP, in));
#else
	return in;
#endif
}

static std::vector<unsigned char> readFile(const std::string &path)
{
	std::vector<unsigned char> data;
#ifdef _WIN32
	std::ifstream f(toWide(CP_UTF8, path).c_str(), std::ifstream::binary);
#else
	std::ifstream f(path, std::ifstream::binary);
#endif
	if (!f)
		return data;
	f.seekg(0, std::ifstream::end);
	data.resize(size_t(f.tellg()));
	f.clear();
	f.seekg(0);
	f.read((char*)data.data(), std::streamsize(data.size()));
	return data;
}

static void writeFile(const std::string &path, const std::vector<unsigned char> &data)
{
#ifdef _WIN32
	std::ofstream f(toWide(CP_UTF8, path).c_str(), std::ofstream::binary);
#else
	std::ofstream f(path.c_str(), std::ofstream::binary);
#endif
	f.write((const char*)data.data(), std::streamsize(data.size()));
}

#if 0
class vectorwrapbuf : public std::streambuf {
public:
	using traits_type = typename std::streambuf::traits_type;
	vectorwrapbuf(std::vector<char> &_vec) : vec(_vec){
		setg(_vec.data(), _vec.data(), _vec.data() + _vec.size());
		setp(_vec.data(), _vec.data() + _vec.size());
	}
	vectorwrapbuf(std::vector<uint8_t> &_vec) : vec(reinterpret_cast<std::vector<char>&>(_vec)){
		setg((char*)_vec.data(), (char*)_vec.data(), (char*)_vec.data() + _vec.size());
		setp((char*)_vec.data(), (char*)_vec.data() + _vec.size());
	}
	pos_type seekpos(pos_type sp, std::ios_base::openmode which) override {
		return seekoff(sp - pos_type(off_type(0)), std::ios_base::beg, which);
	}
	pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override {
		if (which & std::ios_base::in) {
			switch (dir) {
				case std::ios_base::cur: gbump(int(off)); break;
				case std::ios_base::end: setg(eback(), egptr() + off, egptr()); break;
				case std::ios_base::beg: setg(eback(), eback() + off, egptr()); break;
			}
		} else if (which & std::ios_base::out) {
			switch (dir) {
				case std::ios_base::cur: pbump(int(off)); break;
				case std::ios_base::end: setp(eback(), epptr() + off); break;
				case std::ios_base::beg: setp(eback(), eback() + off); break;
			}
		}
		return gptr() - eback();
	}
	std::streamsize xsputn (const char* s, std::streamsize n) override {
		ensure_space(n);
		char *pp = pptr();
		traits_type::copy(pp, s, n);
		std::streambuf::pbump(n);
		return n;
	}
	int overflow (int c) override {
		ensure_space(1);
		return c;
	}
private:
	std::vector<char>& vec;
	void ensure_space(int n) {
		char *dp = vec.data();
		char *pp = pptr();
		char *ep = epptr();
		if((pp + n) > (dp + vec.size())) {
			size_t req_size = pp + n - dp;
			size_t new_size = vec.size() * 2;
			if (new_size < req_size) new_size = req_size;
			vec.resize(new_size);
		}
	}
};
#endif

} // vectorwrapbuf

// A source implementation that always keeps last 16 bytes in tag

struct TaggedSource : public libcdoc::DataSource {
	std::vector<uint8_t> tag;
	libcdoc::DataSource *_src;
	bool _owned;

	TaggedSource(libcdoc::DataSource *src, bool take_ownership, size_t tag_size) : tag(tag_size), _src(src), _owned(take_ownership) {
		tag.resize(tag.size());
		_src->read(tag.data(), tag.size());
	}
	~TaggedSource() {
		if (_owned) delete(_src);
	}

	int seek(size_t pos) override final {
		if (!_src->seek(pos)) return INPUT_STREAM_ERROR;
		if (_src->read(tag.data(), tag.size()) != tag.size()) return INPUT_STREAM_ERROR;
		return libcdoc::OK;
	}

	int64_t read(uint8_t *dst, size_t size) override final {
		uint8_t tmp[tag.size()];
		size_t nread = _src->read(dst, size);
		if (nread >= tag.size()) {
			std::copy(dst + nread - tag.size(), dst + nread, tmp);
			std::copy_backward(dst, dst + nread - tag.size(), dst + nread);
			std::copy(tag.cbegin(), tag.cend(), dst);
			std::copy(tmp, tmp + tag.size(), tag.begin());
		} else {
			std::copy(dst, dst + nread, tmp);
			std::copy(tag.cbegin(), tag.cbegin() + nread, dst);
			std::copy(tag.cbegin() + nread, tag.cend(), tag.begin());
			std::copy(tmp, tmp + nread, tag.end() - nread);
		}
		return nread;
	}

	virtual bool isError() override final {
		return _src->isError();
	}
	virtual bool isEof() override final {
		return _src->isEof();
	}
};

#endif // UTILS_H
