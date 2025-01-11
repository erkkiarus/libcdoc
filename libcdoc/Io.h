#ifndef __IO_H__
#define __IO_H__

#include <libcdoc/Exports.h>

#include <libcdoc/CDoc.h>

#include <filesystem>
#include <fstream>
#include <vector>

namespace libcdoc {

class DataSource;

/**
 * @brief The DataConsumer class
 *
 * An abstact base class for ouput objects
 */
struct CDOC_EXPORT DataConsumer {
	DataConsumer() = default;
	virtual ~DataConsumer() = default;

	/**
	 * @brief write write bytes to output object
	 *
	 * The following invariant holds:
	 * If there was no error then result == size
	 * If there was an error then result < 0
	 * @param src source block
	 * @param size the number of bytes to write
	 * @return size or error code
	 */
	virtual int64_t write(const uint8_t *src, size_t size) = 0;
	/**
	 * @brief close signals the object that writing is finished
	 * @return error code or OK
	 */
	virtual int close() = 0;
	/**
	 * @brief checks whether DataSource is in error state
     * @return error code or OK
	 */
	virtual bool isError() = 0;
	/**
	 * @brief getLastErrorStr get textual description of the last error
	 *
	 * Implementation can decide whether to store the actual error string or
	 * return the generic text based on error code. It is undfined what will
	 * be returned if the last error code is not the one used as the argument.
	 * @param code the last returned error code
	 * @return error text
	 */
	virtual std::string getLastErrorStr(int code) const;

	int64_t write(const std::vector<uint8_t>& src) {
		return write(src.data(), src.size());
	}
	int64_t write(const std::string& src) {
		return write((const uint8_t *) src.data(), src.size());
	}
    /**
     * @brief writeAll reads all data from input object and writes to this
     *
     * Copies all bytes from input source (until EOF or error) to the object. If error occurs
     * while reading source, the source objects' error code is returned.
     * @param src the input DataSource
     * @return the number of bytes copied or error
     */
	int64_t writeAll(DataSource& src);

	DataConsumer (const DataConsumer&) = delete;
	DataConsumer& operator= (const DataConsumer&) = delete;
};

/**
 * @brief The DataSource class
 *
 * An abstact base class for input objects
 */
struct CDOC_EXPORT DataSource {
	DataSource() = default;
	virtual ~DataSource() = default;

	/**
	 * @brief seek set stream input pointer
	 *
	 * Positions the read pointer at the specific distance from the stream start.
	 * If the stream does not support seeking NOT_IMPLEMENTED is returned.
	 * @param pos the position from the beggining of data
	 * @return error code or OK
	 */
	virtual int seek(size_t pos) { return NOT_IMPLEMENTED; }
	/**
	 * @brief read read bytes from input object
	 *
	 * The following invariant holds:
	 * If there is neither error nor eof then result == size
	 * If there is no errors but is eof then 0 <= result <= size
	 * If there is error then result < 0
	 * @param dst the destination block
	 * @param size the number of bytes to read
	 * @return thenumber of bytes read or error code
	 */
	virtual int64_t read(uint8_t *dst, size_t size) = 0;
	virtual bool isError() = 0;
	virtual bool isEof() = 0;
	virtual std::string getLastErrorStr(int code) const;

	int64_t skip(size_t size);
    /**
     * @brief readAll reads all data and writes to output object
     *
     * Copies all bytes (until EOF or error) to the output object. If error occurs
     * while writing data, the destination objects' error code is returned.
     * @param dst the destination DataConsumer
     * @return error code or OK
     */
	int64_t readAll(DataConsumer& dst) {
		return dst.writeAll(*this);
	}

	DataSource (const DataSource&) = delete;
	DataSource& operator= (const DataSource&) = delete;
};

/**
 * @brief An abstract base class for multi-stream consumers
 *
 * A new sub-stream is created by open and finished either by the next open or by closing
 * the whole stream.
 *
 */
struct CDOC_EXPORT MultiDataConsumer : public DataConsumer {
	virtual ~MultiDataConsumer() = default;
    /**
     * @brief open create a new named sub-stream
     *
     * Creates a new named sub-stream. It is up to implementation to handle the name and optional size.
     * @param name the name of sub-stream
     * @param size the size of sub-stream or -1 if unknown during creation time
     * @return error code or OK
     */
	virtual int open(const std::string& name, int64_t size) = 0;
};

/**
 * @brief An abstract base class for multi-stream sources
 */
struct CDOC_EXPORT MultiDataSource : public DataSource {
	virtual size_t getNumComponents() { return NOT_IMPLEMENTED; }
	virtual int next(std::string& name, int64_t& size) = 0;
    int next(FileInfo& info) { return next(info.name, info.size); }
};

struct CDOC_EXPORT ChainedConsumer : public DataConsumer {
	ChainedConsumer(DataConsumer *dst, bool take_ownership) : _dst(dst), _owned(take_ownership) {}
	~ChainedConsumer() {
		if (_owned) delete _dst;
	}
	int64_t write(const uint8_t *src, size_t size) override {
		return _dst->write(src, size);
	}
	int close() override {
		if (_owned) return _dst->close();
        return OK;
	}
	bool isError() override {
		return _dst->isError();
	}
protected:
	DataConsumer *_dst;
	bool _owned;
};

struct CDOC_EXPORT ChainedSource : public DataSource {
	ChainedSource(DataSource *src, bool take_ownership) : _src(src), _owned(take_ownership) {}
	~ChainedSource() {
		if (_owned) delete _src;
	}
	int64_t read(uint8_t *dst, size_t size) {
		return _src->read(dst, size);
	}
	bool isError() {
		return _src->isError();
	}
	bool isEof() {
		return _src->isEof();
	}
protected:
	DataSource *_src;
	bool _owned;
};

struct CDOC_EXPORT IStreamSource : public DataSource {
	IStreamSource(std::istream *ifs, bool take_ownership = false) : _ifs(ifs), _owned(take_ownership) {}
	IStreamSource(const std::string& path);
	~IStreamSource() {
		if (_owned) delete _ifs;
	}

	int seek(size_t pos) {
		_ifs->seekg(pos);
        return bool(_ifs->bad()) ? INPUT_STREAM_ERROR : OK;
	}

	int64_t read(uint8_t *dst, size_t size) {
		_ifs->read((char *) dst, size);
		return (_ifs->bad()) ? INPUT_STREAM_ERROR : _ifs->gcount();
	}

	bool isError() { return _ifs->bad(); }
	bool isEof() { return _ifs->eof(); }
protected:
	std::istream *_ifs;
	bool _owned;
};

struct CDOC_EXPORT OStreamConsumer : public DataConsumer {
	static constexpr int STREAM_ERROR = -500;

	OStreamConsumer(std::ostream *ofs, bool take_ownership = false) : _ofs(ofs), _owned(take_ownership) {}
	OStreamConsumer(const std::string& path);
	~OStreamConsumer() {
		if (_owned) delete _ofs;
	}

	int64_t write(const uint8_t *src, size_t size) {
		_ofs->write((const char *) src, size);
		return (_ofs->bad()) ? OUTPUT_STREAM_ERROR : size;
	}

	int close() {
		_ofs->flush();
        return (_ofs->bad()) ? OUTPUT_STREAM_ERROR : OK;
	}

	bool isError() { return _ofs->bad(); }
protected:
	std::ostream *_ofs;
	bool _owned;
};

struct CDOC_EXPORT VectorSource : public DataSource {
	VectorSource(const std::vector<uint8_t>& data) : _data(data), _ptr(0) {}

    int seek(size_t pos) override {
		if (pos > _data.size()) return INPUT_STREAM_ERROR;
		_ptr = pos;
        return OK;
	}

    int64_t read(uint8_t *dst, size_t size) override {
		size = std::min<size_t>(size, _data.size() - _ptr);
		std::copy(_data.cbegin() + _ptr, _data.cbegin() + _ptr + size, dst);
		_ptr += size;
		return size;
	}

    bool isError() override { return false; }
    bool isEof() override { return _ptr >= _data.size(); }
protected:
	const std::vector<uint8_t>& _data;
	size_t _ptr;
};

struct CDOC_EXPORT VectorConsumer : public DataConsumer {
	VectorConsumer(std::vector<uint8_t>& data) : _data(data) {}
	int64_t write(const uint8_t *src, size_t size) override final {
		_data.insert(_data.end(), src, src + size);
		return size;
	}
    int close() override final { return OK; }
	virtual bool isError() override final { return false; }
protected:
    std::vector<uint8_t>& _data;
};

struct CDOC_EXPORT FileListConsumer : public MultiDataConsumer {
    FileListConsumer(const std::string& base_path) {
		base = base_path;
	}
	int64_t write(const uint8_t *src, size_t size) override final {
		ofs.write((const char *) src, size);
		return (ofs.bad()) ? OUTPUT_STREAM_ERROR : size;
	}
	int close() override final {
		ofs.close();
        return (ofs.bad()) ? OUTPUT_STREAM_ERROR : OK;
	}
	bool isError() override final {
		return ofs.bad();
	}
	int open(const std::string& name, int64_t size) override final {
        std::string fileName;
        if (ofs.is_open()) {
            ofs.close();
        }
        size_t lastSlashPos = name.find_last_of("\\/");
        if (lastSlashPos != std::string::npos)
        {
            fileName = name.substr(lastSlashPos + 1);
        }
        else
        {
            fileName = name;
        }
        std::filesystem::path path(base);
        path /= fileName;
		ofs.open(path.string(), std::ios_base::out);
        return ofs.bad() ? OK : OUTPUT_STREAM_ERROR;
	}

protected:
	std::filesystem::path base;
	std::ofstream ofs;
};

struct CDOC_EXPORT FileListSource : public MultiDataSource {
	FileListSource(const std::string& base, const std::vector<std::string>& files);
	int64_t read(uint8_t *dst, size_t size) override final;
	bool isError() override final;
	bool isEof() override final;
	size_t getNumComponents() override final;
	int next(std::string& name, int64_t& size) override final;
protected:
	std::filesystem::path _base;
	const std::vector<std::string>& _files;
	int64_t _current;
	std::ifstream _ifs;
};

} // namespace libcdoc

#endif // IO_H
