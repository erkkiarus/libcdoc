#include "DDocReader.h"

#include "XmlReader.h"

int
DDOCReader::parse(libcdoc::DataSource *src, libcdoc::MultiDataConsumer *dst)
{
	XMLReader reader(src);
	while(reader.read()) {
		if(reader.isEndElement()) continue;
		// EncryptedData
		if(!reader.isElement("DataFile")) continue;
		std::string name = reader.attribute("Filename");
		std::vector<uint8_t> content = reader.readBase64();
		dst->open(name, content.size());
		dst->write(content.data(), content.size());
		dst->close();
	}
	return !dst->isError();
}

struct FileListConsumer : public libcdoc::MultiDataConsumer {
	std::vector<DDOCReader::File> files;

	explicit FileListConsumer() = default;
	int64_t write(const uint8_t *src, size_t size) override final {
		DDOCReader::File& file = files.back();
		file.data.insert(file.data.end(), src, src + size);
		return size;
	}
	int close() override final { return libcdoc::OK; }
	bool isError() override final { return false; }
	int open(const std::string& name, int64_t size) override final {
		files.push_back({name, "application/octet-stream", {}});
		return libcdoc::OK;
	}
};

std::vector<DDOCReader::File>
DDOCReader::files(const std::vector<uint8_t> &data)
{
	libcdoc::VectorSource src(data);
	FileListConsumer list;
	parse(&src, &list);
	return std::move(list.files);
}
